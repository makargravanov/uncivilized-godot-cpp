import os

def count_lines_in_file(file_path):
    """Counts the number of lines in a file."""
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            return sum(1 for _ in file)
    except Exception as e:
        print(f"Error reading file {file_path}: {e}")
        return 0

def find_files_and_count_lines(directory, extensions):
    """Recursively finds files with given extensions and counts their lines."""
    file_line_counts = {}
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith(extensions):
                file_path = os.path.join(root, file)
                file_line_counts[file_path] = count_lines_in_file(file_path)
    return file_line_counts

def main():
    directory = os.getcwd()  # Current directory
    extensions = ('.h', '.cpp')

    # Find files and count lines
    file_line_counts = find_files_and_count_lines(directory, extensions)

    # Sort files by line count in descending order
    sorted_files = sorted(file_line_counts.items(), key=lambda x: x[1], reverse=True)

    # Print top 10 files by line count
    print("Top 10 files by line count:")
    for file_path, line_count in sorted_files[:10]:
        print(f"{file_path}: {line_count} lines")

    # Print total line count
    total_lines = sum(file_line_counts.values())
    print(f"\nTotal number of lines across all files: {total_lines}")

if __name__ == "__main__":
    main()