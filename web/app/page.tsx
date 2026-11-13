'use client';

import { useEffect, useRef, useState } from 'react';
import { createBrowserClient } from '@supabase/ssr';
import { getSignedUploadUrlAction } from './actions';
import PhotoLibrary from './PhotoLibrary';

const FRAME_WIDTH = 480;
const FRAME_HEIGHT = 800;

function getMimeType(file: File): string {
  if (file.type && file.type !== 'application/octet-stream') return file.type;
  const ext = file.name.slice(file.name.lastIndexOf('.')).toLowerCase();
  const types: Record<string, string> = {
    '.heic': 'image/heic', '.heif': 'image/heif', '.jpg': 'image/jpeg',
    '.jpeg': 'image/jpeg', '.png': 'image/png', '.bmp': 'image/bmp',
  };
  return types[ext] || 'application/octet-stream';
}

export default function Home() {
  const [activeTab, setActiveTab] = useState<'upload' | 'library'>('upload');
  const [file, setFile] = useState<File | null>(null);
  const [previewUrl, setPreviewUrl] = useState('');
  const [imageSize, setImageSize] = useState<{ width: number; height: number } | null>(null);
  const [previewError, setPreviewError] = useState(false);
  const [zoom, setZoom] = useState(1);
  const [horizontal, setHorizontal] = useState(50);
  const [vertical, setVertical] = useState(50);
  const [uploading, setUploading] = useState(false);
  const [status, setStatus] = useState('');
  const imageRef = useRef<HTMLImageElement>(null);
  const dragRef = useRef<{ x: number; y: number; cropX: number; cropY: number } | null>(null);

  useEffect(() => {
    if (!file) return;
    const url = URL.createObjectURL(file);
    setPreviewUrl(url);
    return () => URL.revokeObjectURL(url);
  }, [file]);

  const selectFile = (selected: File | null) => {
    setFile(selected);
    setPreviewUrl('');
    setImageSize(null);
    setPreviewError(false);
    setZoom(1);
    setHorizontal(50);
    setVertical(50);
    setStatus('');
  };

  // The largest portrait crop that fits in the image, reduced by the zoom level.
  const cropWidth = imageSize ? Math.min(imageSize.width, imageSize.height * FRAME_WIDTH / FRAME_HEIGHT) / zoom : 0;
  const cropHeight = cropWidth * FRAME_HEIGHT / FRAME_WIDTH;
  const cropX = imageSize ? (imageSize.width - cropWidth) * horizontal / 100 : 0;
  const cropY = imageSize ? (imageSize.height - cropHeight) * vertical / 100 : 0;

  const moveCrop = (x: number, y: number) => {
    if (!imageSize) return;
    const maxX = imageSize.width - cropWidth;
    const maxY = imageSize.height - cropHeight;
    if (maxX > 0) setHorizontal(Math.max(0, Math.min(100, x / maxX * 100)));
    if (maxY > 0) setVertical(Math.max(0, Math.min(100, y / maxY * 100)));
  };

  const handleSubmit = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const form = e.currentTarget;
    if (!file) return;
    if (!previewError && (!imageSize || !imageRef.current)) {
      setStatus('Please wait for the image preview to load.');
      return;
    }

    const password = new FormData(form).get('password') as string;
    setUploading(true);
    try {
      let uploadFile = file;
      if (imageSize && imageRef.current && !previewError) {
        setStatus('Preparing your crop...');
        const canvas = document.createElement('canvas');
        canvas.width = FRAME_WIDTH;
        canvas.height = FRAME_HEIGHT;
        const context = canvas.getContext('2d');
        if (!context) throw new Error('Could not prepare the image.');
        context.drawImage(imageRef.current, cropX, cropY, cropWidth, cropHeight, 0, 0, FRAME_WIDTH, FRAME_HEIGHT);
        const blob = await new Promise<Blob | null>((resolve) => canvas.toBlob(resolve, 'image/png'));
        if (!blob) throw new Error('Could not save the cropped image.');
        uploadFile = new File([blob], `${file.name.replace(/\.[^/.]+$/, '')}-cropped.png`, { type: 'image/png' });
      }

      setStatus('Verifying password and starting the image processor...');
      const fileType = getMimeType(uploadFile);
      const urlResult = await getSignedUploadUrlAction(password, uploadFile.name, fileType);
      if (!urlResult.success || !urlResult.path || !urlResult.token) {
        throw new Error(urlResult.error || 'Could not prepare the upload.');
      }

      setStatus('Uploading photo...');
      const supabase = createBrowserClient(
        process.env.NEXT_PUBLIC_SUPABASE_URL!,
        process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!
      );
      const { error } = await supabase.storage.from('photos')
        .uploadToSignedUrl(urlResult.path, urlResult.token, uploadFile, { contentType: fileType });
      if (error) throw error;
      form.reset();
      selectFile(null);
      setStatus('Upload successful!');
    } catch (error) {
      setStatus(`Error: ${error instanceof Error ? error.message : 'An unexpected error occurred.'}`);
    } finally {
      setUploading(false);
    }
  };

  return (
    <main className="app-shell">
      <div className="app-panel">
        <header className="app-header"><h1>Photo Frame</h1></header>
        <nav aria-label="Photo management" className="app-nav">
          <button type="button" onClick={() => setActiveTab('upload')} aria-current={activeTab === 'upload' ? 'page' : undefined}
            className={activeTab === 'upload' ? 'active' : ''}>Upload</button>
          <button type="button" onClick={() => setActiveTab('library')} aria-current={activeTab === 'library' ? 'page' : undefined}
            className={activeTab === 'library' ? 'active' : ''}>Photo library</button>
        </nav>
        <div hidden={activeTab !== 'upload'}>
        <div className="section-heading"><h2>Add a photo</h2><p>Choose an image and make it your own.</p></div>
        <form onSubmit={handleSubmit} className="upload-form">
          <label htmlFor="photo-file" className="field-label">Photo</label>
          <input type="file" name="file" accept=".jpg,.jpeg,.png,.bmp,.heic,.heif,image/jpeg,image/png,image/bmp,image/heic,image/heif" required disabled={uploading}
            id="photo-file"
            onChange={(e) => selectFile(e.target.files?.[0] ?? null)}
            className="file-input" />

          {previewUrl && !previewError && (
            <div className="crop-section">
              <p className="helper-text">Drag the box to choose what appears on the frame. Use the arrow keys for finer adjustments.</p>
              <div className="text-center">
              <div className="relative inline-block max-w-full overflow-hidden rounded-lg bg-slate-950 align-top">
                {/* A native image is needed for local object URLs and canvas cropping. */}
                {/* eslint-disable-next-line @next/next/no-img-element */}
                <img ref={imageRef} src={previewUrl} alt="Crop preview" onLoad={(e) => setImageSize({ width: e.currentTarget.naturalWidth, height: e.currentTarget.naturalHeight })}
                  onError={() => setPreviewError(true)}
                  draggable={false} className="block max-h-80 max-w-full object-contain" />
                {imageSize && <div role="button" aria-label={`Crop position: horizontal ${Math.round(horizontal)}%, vertical ${Math.round(vertical)}%. Use arrow keys to move.`} aria-disabled={uploading} tabIndex={uploading ? -1 : 0}
                  onPointerDown={(e) => {
                    if (uploading) return;
                    e.currentTarget.setPointerCapture(e.pointerId);
                    dragRef.current = { x: e.clientX, y: e.clientY, cropX, cropY };
                  }}
                  onPointerMove={(e) => {
                    if (!dragRef.current || !imageRef.current || uploading) return;
                    const rect = imageRef.current.getBoundingClientRect();
                    moveCrop(dragRef.current.cropX + (e.clientX - dragRef.current.x) * imageSize.width / rect.width,
                      dragRef.current.cropY + (e.clientY - dragRef.current.y) * imageSize.height / rect.height);
                  }}
                  onPointerUp={() => { dragRef.current = null; }}
                  onPointerCancel={() => { dragRef.current = null; }}
                  onKeyDown={(e) => {
                    if (uploading) return;
                    const step = e.shiftKey ? 10 : 1;
                    const dx = e.key === 'ArrowLeft' ? -step : e.key === 'ArrowRight' ? step : 0;
                    const dy = e.key === 'ArrowUp' ? -step : e.key === 'ArrowDown' ? step : 0;
                    if (dx || dy) { e.preventDefault(); moveCrop(cropX + dx * imageSize.width / 100, cropY + dy * imageSize.height / 100); }
                  }}
                  className="absolute cursor-move touch-none border-2 border-white shadow-[0_0_0_9999px_rgba(0,0,0,0.55)] focus:outline-2 focus:outline-offset-2 focus:outline-blue-400"
                  style={{ left: `${cropX / imageSize.width * 100}%`, top: `${cropY / imageSize.height * 100}%`, width: `${cropWidth / imageSize.width * 100}%`, height: `${cropHeight / imageSize.height * 100}%` }} />}
              </div>
              </div>
              {imageSize && <fieldset disabled={uploading} className="zoom-control">
                <label className="block">Zoom<input aria-label="Zoom" type="range" min="1" max="4" step="0.01" value={zoom} onChange={(e) => setZoom(Number(e.target.value))} /></label>
              </fieldset>}
            </div>
          )}
          {previewError && <p className="helper-text">Your browser cannot preview this image. You can upload the original and the frame service will crop it automatically, or choose another image.</p>}
          <label htmlFor="upload-password" className="field-label">Password</label>
          <input id="upload-password" type="password" name="password" placeholder="Enter your password" required disabled={uploading}
            className="text-input" />
          <button type="submit" disabled={uploading || (!!file && !imageSize && !previewError)}
            className="primary-button">
            {uploading ? 'Uploading...' : previewError ? 'Upload Original' : 'Upload Photo'}
          </button>
        </form>
        {status && <div role="status" className="status-message">{status}</div>}
        </div>
        {activeTab === 'library' && <PhotoLibrary />}
      </div>
    </main>
  );
}
