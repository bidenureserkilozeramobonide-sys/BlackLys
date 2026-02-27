
import os
import requests
import time
import logging

# B13: API key loaded from env — never hardcode secrets in source
TOPAZ_API_KEY = os.environ.get("TOPAZ_API_KEY", "")
BASE_URL = "https://api.topazlabs.com/image/v1"

logger = logging.getLogger("uvicorn")

class TopazService:
    def __init__(self):
        self.headers = {
            "X-API-Key": TOPAZ_API_KEY
        }

    def process_image(self, file_path: str, model: str = "denoise", output_path: str = None, **kwargs):
        """
        Sends an image to Topaz Labs for processing.
        Models: 'denoise', 'sharpen', 'enhance'
        Extra kwargs: strength, scale_factor, face_enhancement, fix_compression
        """
        if not os.path.exists(file_path):
            raise FileNotFoundError(f"Input file not found: {file_path}")

        # Map to correct API endpoints
        endpoint = model if model in ("denoise", "sharpen", "enhance") else "denoise"
        url = f"{BASE_URL}/{endpoint}" 
        
        logger.info(f"Sending {file_path} to Topaz [{model}] with params: {kwargs}")
        
        try:
            with open(file_path, 'rb') as f:
                files = {'image': f}
                # Pass extra params as form data
                data = {}
                for key, val in kwargs.items():
                    if val is not None:
                        data[key] = str(val) if isinstance(val, bool) else val
                
                response = requests.post(url, headers=self.headers, files=files, data=data)
                
            if response.status_code != 200:
                logger.error(f"Topaz API Error: {response.text}")
                raise Exception(f"Topaz API failed: {response.text}")
                
            # If synchronous, it returns the image binary directly?
            # Or JSON with download URL?
            # Many modern image APIs return the image directly if small, or JSON.
            # Let's handle both.
            
            content_type = response.headers.get('Content-Type', '')
            
            if 'image' in content_type:
                # Direct image return
                if not output_path:
                    output_path = file_path.replace('.', f'_{model}.')
                
                with open(output_path, 'wb') as f:
                    f.write(response.content)
                return output_path
            
            elif 'application/json' in content_type:
                # Async or URL return
                res_json = response.json()
                logger.info(f"Topaz Response JSON: {res_json}")
                # If async, we'd need a loop. For now, let's see what happens.
                # Assuming simple sync for MVP unless we see otherwise.
                return res_json
                
        except Exception as e:
            logger.error(f"Topaz Exception: {e}")
            raise e

topaz_service = TopazService()
