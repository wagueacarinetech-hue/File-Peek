import os
import sys
from openai import OpenAI

sys.stdout.reconfigure(encoding="utf-8")
sys.stderr.reconfigure(encoding="utf-8")

api_key = os.getenv("NVIDIA_API_KEY")

if not api_key:
    print("ERROR: NVIDIA_API_KEY is not set.")
    sys.exit(1)

if len(sys.argv) < 3:
    print("ERROR: Text file and summary mode are required.")
    sys.exit(1)

text_file = sys.argv[1]
mode = sys.argv[2]

try:
    with open(text_file, "r", encoding="utf-8") as file:
        text = file.read()
except Exception as error:
    print(f"ERROR: Could not read text: {error}")
    sys.exit(1)

if not text.strip():
    print("ERROR: The file contains no text.")
    sys.exit(1)

client = OpenAI(
    base_url="https://integrate.api.nvidia.com/v1",
    api_key=api_key
)

# split large files into smaller pieces
def chunk_text(text, chunk_size=12000):
    chunks = []

    for start in range(0, len(text), chunk_size):
        chunks.append(text[start:start + chunk_size])

    return chunks


def summarize(text, mode):
    if mode == "quick":
        instructions = """
Give a short preview of this file.

Include:
- what the file is about
- the main points
- anything especially important

Keep it concise.
"""
        max_tokens = 350

    else:
        instructions = """
Give a detailed summary of this file.

Include:
- what the file is about
- the main sections
- important ideas and details
- conclusions, tasks, or technical concepts

Organize it clearly.
"""
        max_tokens = 1000

    response = client.chat.completions.create(
        model="nvidia/nemotron-3-super-120b-a12b",
        messages=[
            {
                "role": "user",
                "content": instructions + "\n\nDocument:\n" + text
            }
        ],
        temperature=0.3,
        max_tokens=max_tokens
    )

    return response.choices[0].message.content


try:
    chunks = chunk_text(text)

    # small file: summarize normally
    if len(chunks) == 1:
        final_summary = summarize(text, mode)

    # large file: summarize pieces first
    else:
        chunk_summaries = []

        for i, chunk in enumerate(chunks):
            print(
                f"Processing section {i + 1}/{len(chunks)}...",
                file=sys.stderr
            )

            summary = summarize(chunk, "quick")
            chunk_summaries.append(summary)

        combined = "\n\n".join(chunk_summaries)

        # summarize the smaller summaries into one final answer
        final_summary = summarize(combined, mode)

    print(final_summary)

except Exception as error:
    print(f"ERROR: {error}")
    sys.exit(1)