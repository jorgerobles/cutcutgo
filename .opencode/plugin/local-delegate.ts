import type { Plugin } from "@opencode-ai/plugin"

/**
 * Delega los subagentes al modelo local (ornith / llama.cpp en :8090)
 * para gastar menos: cualquier Task(explore/general) que lance el agente
 * principal corre sobre llama.cpp/local-llama-256.
 *
 * Escape: incluir "[main-model]" en el prompt del Task para que ese
 * subagente concreto use el modelo principal.
 *
 * Requiere reiniciar opencode tras editar (la config se carga al arrancar).
 */

const LOCAL_MODEL = "llama.cpp/local-llama-256"

export const LocalDelegate = async () => {
  return {
    config: (cfg: any) => {
      cfg.agent = cfg.agent ?? {}
      const agentsToPin = ["explore", "general"]
      for (const name of agentsToPin) {
        cfg.agent[name] = {
          ...(cfg.agent[name] ?? {}),
          model: LOCAL_MODEL,
        }
      }
    },
    "tool.execute.before": async (input: any, output: any) => {
      if (input.tool !== "task") return
      const prompt = String(output.args?.prompt ?? "")
      if (prompt.includes("[main-model]")) return
      // Refuerzo: si el task tool acepta model en args, fijarlo aquí también
      if ("model" in (output.args ?? {})) {
        output.args.model = LOCAL_MODEL
      }
    },
  }
}

export default LocalDelegate
