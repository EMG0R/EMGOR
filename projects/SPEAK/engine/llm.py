from __future__ import annotations
import os
import threading

try:
    from llama_cpp import Llama
except ImportError:
    Llama = None  # type: ignore[assignment,misc]

DEFAULT_MODEL = os.environ.get(
    "SPEAK_MODEL_PATH",
    "models/Qwen2.5-7B-Instruct-abliterated.Q4_K_M.gguf",
)

_MAX_HISTORY = 10   # keep last N turns — short context = faster TTFT + better cache reuse


class LLMBackend:
    def __init__(self, model_path: str = DEFAULT_MODEL) -> None:
        if Llama is None:
            raise RuntimeError(
                "llama-cpp-python is not installed. Run: pip install llama-cpp-python"
            )
        import platform
        n_gpu = -1 if platform.system() == "Darwin" else 0
        self._llm = Llama(
            model_path=model_path,
            n_ctx=8192,
            n_threads=4,
            n_batch=512,
            n_gpu_layers=n_gpu,
            verbose=False,
        )
        self._lock = threading.Lock()

    def generate(self, system_prompt: str, history: list[dict], user_msg: str) -> str:
        with self._lock:
            trimmed = history[-_MAX_HISTORY:]
            messages = [{"role": "system", "content": system_prompt}]
            messages.extend(trimmed)
            messages.append({"role": "user", "content": user_msg})

            response = self._llm.create_chat_completion(
                messages=messages,
                max_tokens=160,
                temperature=0.8,
                stop=None,
            )
            return response["choices"][0]["message"]["content"].strip()

    def try_generate(self, system_prompt: str, history: list[dict], user_msg: str) -> str | None:
        if not self._lock.acquire(blocking=False):
            return None
        try:
            trimmed = history[-_MAX_HISTORY:]
            messages = [{"role": "system", "content": system_prompt}]
            messages.extend(trimmed)
            messages.append({"role": "user", "content": user_msg})

            response = self._llm.create_chat_completion(
                messages=messages,
                max_tokens=160,
                temperature=0.8,
                stop=None,
            )
            return response["choices"][0]["message"]["content"].strip()
        finally:
            self._lock.release()
