import asyncio
import os
from pathlib import PurePosixPath
from app.config import get_settings
from app.core.supabase import get_supabase
from app.services.processor import smart_crop

async def run_image_pipeline(file_path: str) -> None:
    settings = get_settings()
    supabase = get_supabase()
    
    path_obj = PurePosixPath(file_path)
    
    # make sure the image is from the uploads folder
    if not (len(path_obj.parts) > 1 and path_obj.parts[0] == "uploads"):
        return
    
    # make sure it is actually an image the pipeline can process
    if file_path.endswith("/") or path_obj.suffix.lower() not in [".bmp", ".jpg", ".jpeg", ".png", ".heic", ".heif"]:
        return
    
    try:
        print(f"Downloading: {file_path}")
        raw_bytes = supabase.storage.from_(settings.BUCKET_NAME).download(file_path)

        # check if it is a dict instead of bytes
        if isinstance(raw_bytes, dict):
            print(f"Got an error dictionary")
            return
        
        # make sure it is bytes
        if not isinstance(raw_bytes, (bytes, bytearray)):
            print(f"Wanted bytes but got {type(raw_bytes)}")
            return
        
        # process the img
        target_size = (settings.TARGET_WIDTH, settings.TARGET_HEIGHT)
        processed_bytes = await asyncio.to_thread(smart_crop, raw_bytes=raw_bytes, target_size=target_size)

        # save it to the processed folder as a .bmp
        sub_path = PurePosixPath(*path_obj.parts[1:])  # removes 'uploads'
        destination_path = str(PurePosixPath("processed") / sub_path.with_suffix(".bmp"))

        print(f"Uploading processed output: {destination_path}")
        supabase.storage.from_(settings.BUCKET_NAME).upload(
            path=destination_path,
            file=processed_bytes,
            file_options={"content-type": "image/bmp", "upsert": "true"},
        )
        
        print(f"Done!!!")
        
        # delete it from uploads folder since im on free tier and only have 1 gb
        supabase.storage.from_(settings.BUCKET_NAME).remove([file_path])
    except Exception as e:
        print(f"Got an exception while processing: {e}")
        