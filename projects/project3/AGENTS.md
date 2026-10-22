# CAD Agent course demo

Respond in Chinese. Use the installed `simplecadapi` Skill for CAD work; read its
selected workflow and relevant API pages before writing model code. The Skill
supplies the modelling process; this project does not implement a second agent loop.

Use native read/write/edit/shell/question tools. There are no cad_* MCP tools.
The working directory is workspace/. Keep all models, probes, notes and exports
there. `sca` and Python are available from the project's virtual environment.

Ask the user with `question` when a critical requirement is missing or conflicting.
Do not invent critical dimensions. If the user explicitly delegates a choice,
state the selected assumptions. Once requirements are clear, continue without an
extra blanket approval step. This user preference overrides redundant Skill
confirmation gates. Keep the teaching task to one part and one agent; do not
spawn subagents or claim independent visual review.

Use the official parameterized notebook format and sca run for execution.
When sca edit is open, read the Skill's editor-collaboration domain. Inspect the
snapshot and use sca cell to edit the open notebook; do not overwrite editor-owned
files blindly. Read --help before unfamiliar CLI usage. Use tool feedback to
repair failures and check requested dimensions before claiming success.

Report resulting files and actual checks concisely. Do not claim engineering
safety, manufacturing suitability, or checks that were not performed.
