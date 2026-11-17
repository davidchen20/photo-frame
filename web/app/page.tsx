'use client';

import { useEffect, useState } from 'react';
import { createBrowserClient } from '@supabase/ssr';
import { getSignedUploadUrlsAction } from './actions';
import PhotoLibrary from './PhotoLibrary';

type Crop = { left: number; top: number; right: number; bottom: number };
type Photo = { id: string; file: File; crop: Crop; status: 'pending' | 'uploaded' | 'failed'; error?: string };
const fullImage: Crop = { left: 0, top: 0, right: 100, bottom: 100 };

function getMimeType(file: File): string {
  if (file.type && file.type !== 'application/octet-stream') return file.type;
  const ext = file.name.slice(file.name.lastIndexOf('.')).toLowerCase();
  const types: Record<string, string> = {
    '.heic': 'image/heic', '.heif': 'image/heif', '.jpg': 'image/jpeg',
    '.jpeg': 'image/jpeg', '.png': 'image/png', '.bmp': 'image/bmp',
  };
  return types[ext] || 'application/octet-stream';
}

async function preparePhoto(photo: Photo): Promise<File> {
  const { left, top, right, bottom } = photo.crop;
  if (left === 0 && top === 0 && right === 100 && bottom === 100) return photo.file;
  const url = URL.createObjectURL(photo.file);
  try {
    const image = new Image();
    image.src = url;
    await image.decode();
    const x = Math.floor(image.naturalWidth * left / 100);
    const y = Math.floor(image.naturalHeight * top / 100);
    const width = Math.max(1, Math.round(image.naturalWidth * right / 100) - x);
    const height = Math.max(1, Math.round(image.naturalHeight * bottom / 100) - y);
    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Could not prepare the crop.');
    context.drawImage(image, x, y, width, height, 0, 0, width, height);
    const blob = await new Promise<Blob | null>((resolve) => canvas.toBlob(resolve, 'image/png'));
    if (!blob) throw new Error('Could not save the crop.');
    return new File([blob], `${photo.file.name.replace(/\.[^/.]+$/, '')}-cropped.png`, { type: 'image/png' });
  } finally {
    URL.revokeObjectURL(url);
  }
}

export default function Home() {
  const [activeTab, setActiveTab] = useState<'upload' | 'library'>('upload');
  const [photos, setPhotos] = useState<Photo[]>([]);
  const [activeId, setActiveId] = useState<string | null>(null);
  const [previewUrl, setPreviewUrl] = useState('');
  const [previewError, setPreviewError] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [status, setStatus] = useState('');
  const active = photos.find((photo) => photo.id === activeId);
  const activeFile = active?.file;

  useEffect(() => {
    if (!activeFile) return;
    const url = URL.createObjectURL(activeFile);
    // Object URLs are external resources and must be revoked on selection changes.
    // eslint-disable-next-line react-hooks/set-state-in-effect
    setPreviewUrl(url);
    setPreviewError(false);
    return () => URL.revokeObjectURL(url);
  }, [activeId, activeFile]);

  const updatePhoto = (id: string, change: Partial<Photo>) => {
    setPhotos((items) => items.map((photo) => photo.id === id ? { ...photo, ...change } : photo));
  };

  const handleSubmit = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const pending = photos.filter((photo) => photo.status !== 'uploaded');
    if (!pending.length) return;
    const password = String(new FormData(event.currentTarget).get('password') || '');
    setUploading(true);
    setStatus('Verifying password and starting the image processor...');
    try {
      // Cropped files are PNGs; originals retain their existing format.
      const descriptors = pending.map(({ file, crop }) => {
        const cropped = crop.left !== 0 || crop.top !== 0 || crop.right !== 100 || crop.bottom !== 100;
        return { filename: cropped ? `${file.name.replace(/\.[^/.]+$/, '')}-cropped.png` : file.name,
          contentType: cropped ? 'image/png' : getMimeType(file) };
      });
      const response = await getSignedUploadUrlsAction(password, descriptors);
      if (!response.success || !response.results) throw new Error(response.error || 'Could not prepare uploads.');
      const supabase = createBrowserClient(process.env.NEXT_PUBLIC_SUPABASE_URL!, process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!);
      let succeeded = 0;
      let failed = 0;
      for (let index = 0; index < pending.length; index++) {
        const photo = pending[index];
        setStatus(`Uploading ${index + 1} of ${pending.length}: ${photo.file.name}`);
        try {
          const result = response.results[index];
          if (!result?.path || !result.token) throw new Error(result?.error || 'Could not prepare upload.');
          const uploadFile = await preparePhoto(photo);
          const { error } = await supabase.storage.from('photos').uploadToSignedUrl(result.path, result.token, uploadFile, { contentType: result.contentType });
          if (error) throw error;
          updatePhoto(photo.id, { status: 'uploaded', error: undefined });
          succeeded++;
        } catch (error) {
          updatePhoto(photo.id, { status: 'failed', error: error instanceof Error ? error.message : 'Upload failed.' });
          failed++;
        }
      }
      setStatus(`${succeeded} uploaded${failed ? `, ${failed} failed. You can retry the failed photos.` : '. Photos will be processed after upload.'}`);
    } catch (error) {
      setStatus(`Error: ${error instanceof Error ? error.message : 'Could not start uploads.'}`);
    } finally {
      setUploading(false);
    }
  };

  return (
    <main className="app-shell">
      <div className="app-panel">
        <header className="app-header"><h1>Photo Frame</h1></header>
        <nav aria-label="Photo management" className="app-nav">
          <button type="button" onClick={() => setActiveTab('upload')} aria-current={activeTab === 'upload' ? 'page' : undefined} className={activeTab === 'upload' ? 'active' : ''}>Upload</button>
          <button type="button" onClick={() => setActiveTab('library')} aria-current={activeTab === 'library' ? 'page' : undefined} className={activeTab === 'library' ? 'active' : ''}>Photo library</button>
        </nav>
        <div hidden={activeTab !== 'upload'}>
          <div className="section-heading"><h2>Add photos</h2><p>Choose photos and adjust each crop if you like.</p></div>
          <form onSubmit={handleSubmit} className="upload-form">
            <label htmlFor="photo-file" className="field-label">Photos</label>
            <input type="file" multiple accept=".jpg,.jpeg,.png,.bmp,.heic,.heif,image/jpeg,image/png,image/bmp,image/heic,image/heif" disabled={uploading} id="photo-file" className="file-input"
              onChange={(event) => {
                const selected = Array.from(event.target.files ?? []);
                if (selected.length > 100) { setStatus('Choose up to 100 photos at a time.'); event.target.value = ''; return; }
                const next: Photo[] = selected.map((file) => ({ id: crypto.randomUUID(), file, crop: { ...fullImage }, status: 'pending' }));
                setPhotos(next); setActiveId(next[0]?.id ?? null); setStatus('');
              }} />
            {photos.length > 0 && <ul className="upload-queue" aria-label="Selected photos">
              {photos.map((photo, index) => <li key={photo.id}>
                <button type="button" disabled={uploading} className={photo.id === activeId ? 'selected-photo' : ''} aria-pressed={photo.id === activeId} onClick={() => { if (photo.id !== activeId) { setPreviewUrl(''); setActiveId(photo.id); } }}>
                  {index + 1}. {photo.file.name} {photo.status === 'uploaded' ? '✓ Uploaded' : photo.status === 'failed' ? '— Failed' : ''}
                </button>
                {photo.error && <span className="photo-error">{photo.error}</span>}
              </li>)}
            </ul>}
            {active && previewUrl && !previewError && <div className="crop-section">
              <p className="helper-text">Adjust the edges to select an area. The processor will fit that area to the frame, so some of it may be cropped again.</p>
              <div className="text-center"><div className="relative inline-block max-w-full overflow-hidden rounded-lg bg-slate-950 align-top">
                {/* eslint-disable-next-line @next/next/no-img-element */}
                <img src={previewUrl} alt={`Crop preview for ${active.file.name}`} onError={() => setPreviewError(true)} draggable={false} className="block max-h-80 max-w-full object-contain" />
                <div aria-hidden="true" className="absolute border-2 border-white shadow-[0_0_0_9999px_rgba(0,0,0,0.55)] pointer-events-none" style={{ left: `${active.crop.left}%`, top: `${active.crop.top}%`, width: `${active.crop.right - active.crop.left}%`, height: `${active.crop.bottom - active.crop.top}%` }} />
              </div></div>
              <fieldset disabled={uploading || active.status === 'uploaded'} className="crop-controls">
                <legend>Crop edges</legend>
                {(['left', 'top', 'right', 'bottom'] as const).map((edge) => <label key={edge}>{edge[0].toUpperCase() + edge.slice(1)}: {active.crop[edge]}%
                  <input type="range" min={edge === 'right' ? active.crop.left + 1 : edge === 'bottom' ? active.crop.top + 1 : 0} max={edge === 'left' ? active.crop.right - 1 : edge === 'top' ? active.crop.bottom - 1 : 100} value={active.crop[edge]} onChange={(event) => updatePhoto(active.id, { crop: { ...active.crop, [edge]: Number(event.target.value) } })} />
                </label>)}
                <button type="button" onClick={() => updatePhoto(active.id, { crop: { ...fullImage } })}>Reset crop</button>
              </fieldset>
            </div>}
            {active && previewError && <p className="helper-text">Your browser cannot preview this image. The original will be uploaded and cropped automatically by the frame service.</p>}
            <label htmlFor="upload-password" className="field-label">Password</label>
            <input id="upload-password" type="password" name="password" placeholder="Enter your password" required disabled={uploading} className="text-input" />
            <button type="submit" disabled={uploading || !photos.some((photo) => photo.status !== 'uploaded')} className="primary-button">{uploading ? 'Uploading...' : photos.some((photo) => photo.status === 'failed') ? 'Upload remaining photos' : 'Upload photos'}</button>
          </form>
          {status && <div role="status" className="status-message">{status}</div>}
        </div>
        {activeTab === 'library' && <PhotoLibrary />}
      </div>
    </main>
  );
}
