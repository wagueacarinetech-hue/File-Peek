import os
import sys
import re
from openai import OpenAI


sys.stdout.reconfigure(
    encoding="utf-8"
)

sys.stderr.reconfigure(
    encoding="utf-8"
)


MODEL = "nvidia/nemotron-3-super-120b-a12b"

QUICK_CHARACTER_LIMIT = 16000


def get_client():
    api_key = os.getenv(
        "NVIDIA_API_KEY"
    )

    if not api_key:
        raise RuntimeError(
            "NVIDIA_API_KEY is not set."
        )

    return OpenAI(
        base_url="https://integrate.api.nvidia.com/v1",
        api_key=api_key
    )


def clean_model_text(text):
    if not text:
        return ""

    replacements = {
        "\u2014": "-",
        "\u2013": "-",
        "\u2018": "'",
        "\u2019": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u2022": "-",
        "\u00a0": " ",
        "\u2026": "...",

        "â€”": "-",
        "â€“": "-",
        "â€™": "'",
        "â€˜": "'",
        "â€œ": '"',
        "â€¢": "-",
        "Â ": " ",
        "Â": "",
        "ï¿½": ""
    }

    for bad, good in replacements.items():
        text = text.replace(
            bad,
            good
        )

    text = text.replace(
        "\ufffd",
        ""
    )

    text = text.replace(
        "**",
        ""
    )

    text = text.replace(
        "__",
        ""
    )

    text = re.sub(
        r"[ \t]+",
        " ",
        text
    )

    text = re.sub(
        r"\n{3,}",
        "\n\n",
        text
    )

    return text.strip()


def chunk_text(
    text,
    chunk_size=12000
):
    return [
        text[start:start + chunk_size]

        for start in range(
            0,
            len(text),
            chunk_size
        )
    ]


def call_model(
    client,
    text,
    mode
):
    if mode == "quick":

        instructions = """
You are writing the Quick Summary for FilePeek.

FilePeek helps users identify poorly named files without opening them.

Return only one short final paragraph.

Immediately identify what this file is or what its purpose is.

Then mention the most useful contents, topics, tasks, ideas, or details that would help someone recognize whether this is the file they are looking for.

Keep the result to 2 to 4 concise sentences.

Do not include reasoning.
Do not describe how you wrote the answer.
Do not use headings.
Do not use bullet points.
Do not use numbered lists.
Do not use Markdown.
Do not use asterisks.
Do not write "Preview of the file".
Do not write "What it is".
Do not write "Main points".
Do not write "Important details".

Use normal plain-text punctuation.

Return only the finished preview paragraph.
"""

        max_tokens = 220

    else:

        instructions = """
Give a detailed, easy-to-read summary of this file.

Explain what the file is, the important sections or topics, the major ideas and details, and any important tasks, conclusions, results, or technical concepts.

The goal is to help someone understand the file without immediately opening and reading the entire thing.

Return only the final user-facing summary.
Do not include reasoning or drafting.

Use clear formatting and simple punctuation.
"""

        max_tokens = 900


    response = (
        client
        .chat
        .completions
        .create(
            model=MODEL,

            messages=[
                {
                    "role": "system",
                    "content":
                        "Return only final user-facing content. "
                        "Do not reveal reasoning, planning, drafting, "
                        "or intermediate steps."
                },
                {
                    "role": "user",
                    "content":
                        instructions +
                        "\n\nFILE CONTENT:\n" +
                        text
                }
            ],

            # FilePeek does not need reasoning for summarization.
            reasoning_effort="none",

            temperature=0.2,

            max_tokens=max_tokens
        )
    )


    result = (
        response
        .choices[0]
        .message
        .content
    )


    return clean_model_text(
        result
    )


def summarize_text(
    text,
    mode="quick"
):
    if not text or not text.strip():
        raise RuntimeError(
            "The file contains no readable text."
        )


    mode = mode.lower()


    if mode not in (
        "quick",
        "detailed"
    ):
        raise RuntimeError(
            "Summary mode must be quick or detailed."
        )


    client = get_client()


    if mode == "quick":

        quick_text = text[
            :QUICK_CHARACTER_LIMIT
        ]


        result = call_model(
            client,
            quick_text,
            "quick"
        )


        if not result:
            raise RuntimeError(
                "The model returned an empty summary."
            )


        return result


    chunks = chunk_text(
        text
    )


    if len(chunks) == 1:

        result = call_model(
            client,
            text,
            "detailed"
        )


        if not result:
            raise RuntimeError(
                "The model returned an empty summary."
            )


        return result


    chunk_summaries = []


    for chunk in chunks:

        summary = call_model(
            client,
            chunk,
            "quick"
        )


        if summary:
            chunk_summaries.append(
                summary
            )


    if not chunk_summaries:
        raise RuntimeError(
            "The model returned no usable summary."
        )


    combined = "\n\n".join(
        chunk_summaries
    )


    result = call_model(
        client,
        combined,
        "detailed"
    )


    if not result:
        raise RuntimeError(
            "The model returned an empty summary."
        )


    return result


def main():

    if len(sys.argv) < 3:

        print(
            "ERROR: Text file and summary mode are required."
        )

        sys.exit(1)


    text_file = sys.argv[1]

    mode = sys.argv[2]


    try:

        with open(
            text_file,
            "r",
            encoding="utf-8",
            errors="replace"
        ) as file:

            text = file.read()


        summary = summarize_text(
            text,
            mode
        )


        print(
            summary
        )


    except Exception as error:

        print(
            f"ERROR: {error}"
        )

        sys.exit(1)


if __name__ == "__main__":
    main()