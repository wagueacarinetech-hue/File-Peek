import sys
import csv
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

from pypdf import PdfReader
from docx import Document


def extract_pdf(path):
    reader = PdfReader(path)
    text = ""

    for page in reader.pages:
        page_text = page.extract_text()

        if page_text:
            text += page_text + "\n"

    return text


def extract_docx(path):
    document = Document(path)
    text = ""

    for paragraph in document.paragraphs:
        text += paragraph.text + "\n"

    return text


def extract_txt(path):
    with open(
        path,
        "r",
        encoding="utf-8",
        errors="replace"
    ) as file:
        return file.read()


def extract_markdown(path):
    with open(
        path,
        "r",
        encoding="utf-8",
        errors="replace"
    ) as file:
        return file.read()


def extract_csv(path):
    rows = []

    with open(
        path,
        "r",
        encoding="utf-8-sig",
        errors="replace",
        newline=""
    ) as file:
        reader = csv.reader(file)

        for row in reader:
            rows.append(row)

    if not rows:
        return ""

    headers = rows[0]

    text = "Columns: " + ", ".join(headers) + "\n\n"

    for row_number, row in enumerate(
        rows[1:],
        start=1
    ):
        text += f"Row {row_number}:\n"

        for index, value in enumerate(row):
            if index < len(headers):
                column_name = headers[index]
            else:
                column_name = f"Column {index + 1}"

            text += (
                f"{column_name}: {value}\n"
            )

        text += "\n"

    return text


def extract_file(path):
    extension = Path(path).suffix.lower()

    if extension == ".pdf":
        return extract_pdf(path)

    elif extension == ".docx":
        return extract_docx(path)

    elif extension == ".txt":
        return extract_txt(path)

    elif extension == ".csv":
        return extract_csv(path)

    elif extension == ".md":
        return extract_markdown(path)

    else:
        raise ValueError(
            f"Unsupported file type: {extension}"
        )


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("ERROR: No file path provided.")
        sys.exit(1)

    file_path = sys.argv[1]

    try:
        extracted_text = extract_file(
            file_path
        )

        print(extracted_text)

    except Exception as error:
        print(
            f"ERROR: {error}"
        )

        sys.exit(1)