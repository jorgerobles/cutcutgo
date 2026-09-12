#!/usr/bin/env bash
# llm.sh — delegación de tareas auxiliares al modelo local (Ornith 9B @ :8090)
# Uso:   echo "resume esto: ..." | tools/llm.sh [system_prompt]
set -euo pipefail
PORT="${LLM_PORT:-8090}"
MODEL="${LLM_MODEL:-/models/Ornith-1.5-9B-Q8_0.gguf}"
SYS="${1:-Eres un asistente técnico conciso. Responde en español, sin rodeos.}"
PROMPT="$(cat)"
jq -n --arg m "$MODEL" --arg s "$SYS" --arg p "$PROMPT" \
  '{model:$m, messages:[{role:"system",content:$s},{role:"user",content:$p}], stream:false}' |
curl -s --max-time 120 "http://localhost:${PORT}/v1/chat/completions" -d @- |
python3 -c 'import json,sys; print(json.load(sys.stdin)["choices"][0]["message"]["content"])'
