#!/usr/bin/env python3
import os
import sys
import struct

TITLES = {
    0x415608C6: 'Assassins Creed 1',
    0x454109C6: 'Assassins Creed 1 (EU)',
    0x41560817: 'Assassins Creed 2',
    0x415608CB: 'Assassins Creed Brotherhood',
    0x41560911: 'Assassins Creed 3',
    0x41560904: 'COD Ghosts',
    0x545407E6: 'GTA IV',
    0x545407F2: 'GTA V',
    0x4D5307EA: 'Halo 3',
    0x4D53084D: 'Halo 4',
    0x4D5307EB: 'Halo ODST',
    0x555308B7: 'Red Dead Redemption',
    0x55530833: 'Red Dead Redemption GOTY',
    0x555308C2: 'Borderlands 2',
    0x4B5607E4: 'Call of Duty MW3',
    0x4E4D07FC: 'FIFA',
    0x4C4107D7: 'Dark Souls',
    0x425607D4: 'Batman Arkham City',
    0x545107F6: 'Minecraft',
    0x41560A28: 'COD Black Ops 2',
    0x415605F0: 'COD Black Ops',
    0x415605CA: 'COD MW2',
    0x4D5307E6: 'Halo 3 ODST',
    0x555308BA: 'GTA V',
    0x545408A7: 'GTA V',
    0x4D530919: 'Halo 4',
    0x45410950: 'Battlefield',
}

def is_valid_stfs(data):
    """
    Strictly validate an STFS header.
    A real STFS container must pass ALL these checks.
    """
    if len(data) < 0x500:
        return False

    # check magic
    magic = data[0:4]
    if magic not in [b'CON ', b'LIVE', b'PIRS']:
        return False

    # title ID at 0x360 must be non-zero and realistic
    tid = struct.unpack('>I', data[0x360:0x364])[0]
    if tid == 0:
        return False

    # content type at 0x344 must be a known value
    # 0x01 = saved game, 0x02 = marketplace content, 0x0003 = publisher content
    content_type = struct.unpack('>I', data[0x344:0x348])[0]
    valid_types = {0x00000001, 0x00000002, 0x00000003, 0x00000004,
                   0x000D0000, 0x00090000, 0x00040000, 0x000B0000}
    if content_type not in valid_types:
        return False

    # version field at 0x364 should be 0 or 1
    version = struct.unpack('>I', data[0x364:0x368])[0]
    if version > 2:
        return False

    return True

def parse_stfs_header(data):
    """Parse a validated STFS header."""
    magic = data[0:4]
    tid = struct.unpack('>I', data[0x360:0x364])[0]

    # display name at 0x411 (UTF-16 BE)
    name_raw = data[0x411:0x411+0x80]
    try:
        display_name = name_raw.decode('utf-16-be').rstrip('\x00').strip()
    except:
        display_name = ''

    # gamertag at 0x406 (ASCII)
    try:
        gamertag = data[0x406:0x420].decode('ascii', errors='ignore')
        gamertag = ''.join(c for c in gamertag if c.isprintable()).strip()
    except:
        gamertag = ''

    # date string at 0x020
    try:
        date_str = data[0x020:0x02a].decode('ascii', errors='ignore').strip()
        date_str = ''.join(c for c in date_str if c.isprintable())
    except:
        date_str = ''

    title_name = TITLES.get(tid, f'Unknown (0x{tid:08X})')

    return {
        'magic': magic.decode('ascii'),
        'title_id': tid,
        'title_name': title_name,
        'display_name': display_name,
        'gamertag': gamertag,
        'date': date_str,
    }

def scan_drive(drive_path):
    """Scan drive for valid STFS containers."""
    print(f"\nSaveHarbor - scanning {drive_path}")
    print("This may take a few minutes...\n")

    saves = []

    try:
        f = open(drive_path, 'rb')
    except PermissionError:
        print("Error: permission denied. Try running with sudo!")
        sys.exit(1)
    except FileNotFoundError:
        print(f"Error: drive {drive_path} not found!")
        sys.exit(1)

    f.seek(0, 2)
    drive_size = f.tell()
    print(f"Drive size: {drive_size // (1024**3):.1f} GB")

    chunk_size = 1024 * 1024  # 1MB chunks
    offset = 0
    scanned_gb = 0

    while offset < drive_size:
        # clean progress bar on a single line
        gb_done = offset / (1024**3)
        total_gb = drive_size / (1024**3)
        pct = (offset / drive_size) * 100
        bar = '█' * int(pct / 5) + '░' * (20 - int(pct / 5))
        print(f'\r[{bar}] {pct:5.1f}% | {gb_done:.1f}/{total_gb:.1f} GB | Found: {len(saves)}',
              end='', flush=True)

        try:
            f.seek(offset)
            chunk = f.read(chunk_size)
        except:
            offset += chunk_size
            continue

        for magic in [b'CON ', b'LIVE', b'PIRS']:
            pos = 0
            while True:
                idx = chunk.find(magic, pos)
                if idx == -1:
                    break
                abs_offset = offset + idx

                # read full header for validation
                f.seek(abs_offset)
                header_data = f.read(0x500)

                # strict validation — filters out game data false positives
                if is_valid_stfs(header_data):
                    if not any(s['offset'] == abs_offset for s in saves):
                        info = parse_stfs_header(header_data)
                        info['offset'] = abs_offset
                        saves.append(info)

                pos = idx + 1

        offset += chunk_size

    print(f'\r[{"█" * 20}] 100.0% | {drive_size/(1024**3):.1f} GB | Found: {len(saves)}')
    print(f"\nScan complete! Found {len(saves)} valid save files.\n")
    f.close()
    return saves

def extract_save(drive_path, save_info, output_dir):
    """Extract a single save file."""
    os.makedirs(output_dir, exist_ok=True)

    safe_name = save_info['title_name'].replace(' ', '_')
    safe_name = ''.join(c for c in safe_name if c.isalnum() or c in '_-')
    filename = f"{safe_name}_{save_info['offset']:x}.stfs"
    output_path = os.path.join(output_dir, filename)

    print(f"\nExtracting: {save_info['title_name']}")
    print(f"Gamertag:   {save_info['gamertag']}")
    print(f"Date:       {save_info['date']}")
    print(f"To:         {output_path}")

    try:
        f = open(drive_path, 'rb')
        f.seek(save_info['offset'])
        data = f.read(4 * 1024 * 1024)
        f.close()

        with open(output_path, 'wb') as out:
            out.write(data)

        print(f"Done! ({len(data) // 1024}KB extracted)")
        return output_path

    except Exception as e:
        print(f"Error: {e}")
        return None

def main():
    if len(sys.argv) < 2:
        print("Usage: sudo python3 Data_extractor.py <drive>")
        print("Example: sudo python3 Data_extractor.py /dev/sda")
        sys.exit(1)

    drive_path = sys.argv[1]
    output_dir = os.path.expanduser('~/SaveHarbor_Saves')

    saves = scan_drive(drive_path)

    if not saves:
        print("No valid save files found.")
        sys.exit(0)

    # display results table
    print("=" * 65)
    print(f"{'#':<4} {'Game':<30} {'Gamertag':<12} {'Date':<12} {'Type'}")
    print("=" * 65)
    for i, save in enumerate(saves):
        print(f"{i+1:<4} {save['title_name']:<30} {save['gamertag']:<12} "
              f"{save['date']:<12} {save['magic']}")
    print("=" * 65)

    print(f"\nSaves will go to: {output_dir}")
    print("\nEnter a number to extract, 'all' for everything, or 'q' to quit")

    while True:
        choice = input("\nYour choice: ").strip().lower()

        if choice == 'q':
            break
        elif choice == 'all':
            print(f"\nExtracting all {len(saves)} saves...")
            for save in saves:
                extract_save(drive_path, save, output_dir)
            print(f"\nDone! All saves in {output_dir}")
            break
        elif choice.isdigit():
            idx = int(choice) - 1
            if 0 <= idx < len(saves):
                path = extract_save(drive_path, saves[idx], output_dir)
                if path:
                    print(f"\nTo use with Xenia: copy to ~/.config/Xenia/content/")
            else:
                print(f"Enter a number between 1 and {len(saves)}")
        else:
            print("Invalid input.")

if __name__ == '__main__':
    main()
