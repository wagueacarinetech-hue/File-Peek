import sys
import os
import base64
from pathlib import Path

from openai import OpenAI

from extractor import extract_file
from summarizer import summarize_text


IMAGE_EXTENSIONS = {
    ".png",
    ".jpg",
    ".jpeg"
}


VISION_MODEL = (
    "nvidia/nemotron-nano-12b-v2-vl"
)


def summarize_image(
    file_path,
    mode
):
    api_key = os.getenv(
        "NVIDIA_API_KEY"
    )

    if not api_key:
        raise RuntimeError(
            "NVIDIA_API_KEY is not set."
        )

    extension = (
        Path(file_path)
        .suffix
        .lower()
    )

    if extension == ".png":
        mime_type = "image/png"
    else:
        mime_type = "image/jpeg"

    with open(
        file_path,
        "rb"
    ) as image_file:
        encoded_image = (
            base64
            .b64encode(
                image_file.read()
            )
            .decode("utf-8")
        )

    image_url = (
        f"data:{mime_type};base64,"
        f"{encoded_image}"
    )

    if mode == "quick":
        instructions = """
Give a short preview of this image.

Include:
- what the image shows
- the most important visible information
- important text, UI, chart, diagram, or error if present

Keep it concise.
Use simple plain-text punctuation.
"""
        max_tokens = 300

    else:
        instructions = """
Give a detailed explanation of this image.

Include:
- what is visible
- important objects and details
- visible text or UI elements
- charts, diagrams, or errors if present
- useful context someone would want before opening the image

Organize it clearly.
Use simple plain-text punctuation.
"""
        max_tokens = 800

    client = OpenAI(
        base_url=(
            "https://integrate.api.nvidia.com/v1"
        ),
        api_key=api_key
    )

    response = (
        client
        .chat
        .completions
        .create(
            model=VISION_MODEL,
            messages=[
                {
                    "role": "user",
                    "content": [
                        {
                            "type": "text",
                            "text": instructions
                        },
                        {
                            "type": "image_url",
                            "image_url": {
                                "url": image_url
                            }
                        }
                    ]
                }
            ],
            temperature=0.2,
            max_tokens=max_tokens
        )
    )

    return (
        response
        .choices[0]
        .message
        .content
    )


def summarize_document(
    file_path,
    mode
):
    text = extract_file(
        file_path
    )

    if not text or not text.strip():
        raise RuntimeError(
            "No readable text was found in this file."
        )

    return summarize_text(
        text,
        mode
    )


def main():
    if len(sys.argv) < 3:
        print(
            "ERROR: File path and summary mode are required."
        )
        sys.exit(1)

    file_path = sys.argv[1]

    mode = (
        sys.argv[2]
        .lower()
    )

    if mode not in (
        "quick",
        "detailed"
    ):
        print(
            "ERROR: Summary mode must be quick or detailed."
        )
        sys.exit(1)

    if not os.path.isfile(
        file_path
    ):
        print(
            f"ERROR: File not found: {file_path}"
        )
        sys.exit(1)

    try:
        extension = (
            Path(file_path)
            .suffix
            .lower()
        )

        if extension in IMAGE_EXTENSIONS:
            summary = summarize_image(
                file_path,
                mode
            )

        else:
            summary = summarize_document(
                file_path,
                mode
            )

        print(summary)

    except Exception as error:
        print(
            f"ERROR: {error}"
        )
        sys.exit(1)


if __name__ == "__main__":
    main()