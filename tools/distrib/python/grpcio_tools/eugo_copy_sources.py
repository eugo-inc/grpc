#!/usr/bin/env python3
import pathlib
import shutil
import sys


def main():
    print("[EUGO]: Starting copy of gRPC sources...")

    # Get current working directory (Meson runs from build dir)
    current_dir = pathlib.Path.cwd()
    print(f"[EUGO]: Working directory: {current_dir}")

    success_count = 0
    for source, destination in [("include", "grpc_root/include"), ("src/compiler", "grpc_root/src/compiler")]:
        source_directory = pathlib.Path(f"../../../../{source}").resolve()
        destination_directory = current_dir / destination

        print(f"[EUGO]: Copying {source_directory} -> {destination_directory}")

        # Check if source exists
        if not source_directory.exists():
            print(f"[EUGO]: ERROR - Source directory not found: {source_directory}")
            continue

        try:
            # Ensure parent directory exists
            destination_directory.parent.mkdir(parents=True, exist_ok=True)

            # Copy the directory
            shutil.copytree(source_directory, destination_directory, dirs_exist_ok=True)

            if destination_directory.exists():
                print(f"[EUGO]: ✓ Successfully copied {source_directory} -> {destination_directory}")
                success_count += 1
            else:
                print(f"[EUGO]: ✗ Failed to copy {source_directory} -> {destination_directory}")

        except Exception as error:
            print(f"[EUGO]: ✗ Error copying {source_directory} -> {destination_directory}: {error}")

    if success_count == 2:
        # Create stamp file in current directory (build dir)
        stamp_file = current_dir / "grpc_root_copied.stamp"
        stamp_file.touch()
        print(f"[EUGO]: ✓ Created stamp file: {stamp_file}")
        print("[EUGO]: Copy operation completed successfully!")
        return 0
    else:
        print(f"[EUGO]: ✗ Copy operation failed. Only {success_count}/2 directories copied.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
