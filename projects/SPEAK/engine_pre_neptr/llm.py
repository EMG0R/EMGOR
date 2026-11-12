from __future__ import annotations
import os

try:
    from llama_cpp import Llama
except ImportError:
    Llama = None  # type: ignore[assignment,misc]

DEFAULT_MODEL = os.environ.get(
    "SPEAK_MODEL_PATH",
    "models/Hermes-3-Llama-3.1-8B.Q4_K_M.gguf",
)

_MAX_HISTORY = 20   # keep last N turns to avoid context overflow


class LLMBackend:
    def __init__(self, model_path: str = DEFAULT_MODEL) -> None:
        if Llama is None:
            raise RuntimeError(
                "llama-cpp-python is not installed. Run: pip install llama-cpp-python"
            )
        self._llm = Llama(
            model_path=model_path,
            n_ctx=4096,
            n_threads=3,
            n_batch=512,
            verbose=False,
        )

    def generate(self, system_prompt: str, history: list[dict], user_msg: str) -> str:
        trimmed = history[-_MAX_HISTORY:]
        messages = [{"role": "system", "content": system_prompt}]
        messages.extend(trimmed)
        messages.append({"role": "user", "content": user_msg})

        response = self._llm.create_chat_completion(
            messages=messages,
            max_tokens=512,
            temperature=0.8,
            stop=None,
        )
        return response["choices"][0]["message"]["content"].strip()
