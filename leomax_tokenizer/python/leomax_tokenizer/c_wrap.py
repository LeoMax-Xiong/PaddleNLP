from . import core_tokenizers as C
from .tokenizer_impl.data_tokenizer import InputSequence
class Tokenizer:
    def __init__(self, model):
        self._tokenizer = None
        if model is not None:
            self._tokenizer = C.Tokenizer(model._model)
            
    def encode(self, 
               text: InputSequence, 
               pair):
        return self._tokenizer.encode(text)
    
    def token_to_id(self, token):
        return self._tokenizer.token_to_id(token)
    
    def add_special_tokens(self, tokens):
        print(f"add special tokens: f{tokens}")
        self._tokenizer.add_special_tokens(tokens)