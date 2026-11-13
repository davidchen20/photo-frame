'use client';

import { useState } from 'react';
import { deletePhotoAction, listPhotosAction } from './actions';

type Photo = { id: string | number; storage_path: string; url: string | null };

export default function PhotoLibrary() {
  const [password, setPassword] = useState('');
  const [photos, setPhotos] = useState<Photo[] | null>(null);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState('');

  const loadPhotos = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    setBusy(true);
    setMessage('');
    try {
      const result = await listPhotosAction(password);
      if (!result.success || !result.photos) throw new Error(result.error || 'Could not load photos.');
      setPhotos(result.photos);
    } catch (error) {
      setPhotos(null);
      setMessage(error instanceof Error ? error.message : 'Could not load photos.');
    } finally {
      setBusy(false);
    }
  };

  const deletePhoto = async (photo: Photo) => {
    if (!window.confirm(`Delete ${photo.storage_path.split('/').pop()} from the cloud? The frame may still have a cached copy.`)) return;
    setBusy(true);
    setMessage('');
    try {
      const result = await deletePhotoAction(password, photo.id);
      if (!result.success) throw new Error(result.error || 'Could not delete photo.');
      setPhotos((current) => current?.filter((item) => item.id !== photo.id) ?? null);
      setMessage('Photo deleted from the cloud.');
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Could not delete photo.');
    } finally {
      setBusy(false);
    }
  };

  return (
    <section aria-labelledby="library-heading">
      <div className="section-heading"><h2 id="library-heading">Photo library</h2>
      <p>Manage your photos in the cloud. Photos already on the frame remain on its SD card.</p></div>
      <form onSubmit={loadPhotos} className="library-form">
        <input type="password" value={password} onChange={(e) => { setPassword(e.target.value); setPhotos(null); }} required disabled={busy}
          aria-label="Password for photo library" placeholder="Enter your password" className="text-input" />
        <button disabled={busy} className="primary-button">{busy ? 'Working...' : 'Load photos'}</button>
      </form>
      {message && <p role="status" className="status-message">{message}</p>}
      {photos && (photos.length === 0 ? <p className="status-message">No photos found.</p> :
        <ul className="photo-grid">
          {photos.map((photo) => <li key={photo.id} className="photo-card">
            {photo.url ? (
              // Signed URLs are generated for private storage objects.
              // eslint-disable-next-line @next/next/no-img-element
              <img src={photo.url} alt={photo.storage_path.split('/').pop() || 'Photo'} className="aspect-[3/5] w-full rounded object-contain bg-slate-900" />
            ) : <div className="flex aspect-[3/5] items-center justify-center rounded bg-slate-900 p-2 text-center text-xs text-slate-400">Preview unavailable</div>}
            <p className="photo-name" title={photo.storage_path}>{photo.storage_path.split('/').pop()}</p>
            <button type="button" disabled={busy} onClick={() => deletePhoto(photo)} className="delete-button">Delete photo</button>
          </li>)}
        </ul>)}
    </section>
  );
}
