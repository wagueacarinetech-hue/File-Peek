import sys
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


def extract_file(path):
    extension = Path(path).suffix.lower()

    if extension == ".pdf":
        return extract_pdf(path)

    elif extension == ".docx":
        return extract_docx(path)

    else:
        raise ValueError(f"Unsupported file type: {extension}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("ERROR: No file path provided.")
        sys.exit(1)

    file_path = sys.argv[1]

    try:
        extracted_text = extract_file(file_path)
        print(extracted_text)

    except Exception as error:
        print(f"ERROR: {error}")
        sys.exit(1)