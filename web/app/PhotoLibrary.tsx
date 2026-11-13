'use client';

import { useState } from 'react';
import { deleteOrphanPhotoAction, deletePhotoAction, listPhotosAction } from './actions';

type Photo = { id: string | number; storage_path: string; url: string | null };
type Frame = { id: string; name: string; reportedAt: string | null; photoIds: string[]; pendingIds: string[] };

export default function PhotoLibrary() {
  const [password, setPassword] = useState('');
  const [photos, setPhotos] = useState<Photo[] | null>(null);
  const [frames, setFrames] = useState<Frame[]>([]);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState('');

  const refresh = async () => {
    const result = await listPhotosAction(password);
    if (!result.success || !result.photos || !result.frames) throw new Error(result.error || 'Could not load photos.');
    setPhotos(result.photos);
    setFrames(result.frames);
  };

  const loadPhotos = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    setBusy(true);
    setMessage('');
    try {
      await refresh();
    } catch (error) {
      setPhotos(null);
      setMessage(error instanceof Error ? error.message : 'Could not load photos.');
    } finally {
      setBusy(false);
    }
  };

  const deletePhoto = async (photo: Photo) => {
    if (!window.confirm(`Delete ${photo.storage_path.split('/').pop()} from the cloud and all active frames? Offline frames will delete it when they reconnect.`)) return;
    setBusy(true);
    setMessage('');
    try {
      const result = await deletePhotoAction(password, photo.id);
      if (!result.success) throw new Error(result.error || 'Could not delete photo.');
      await refresh();
      setMessage('Photo deleted from the cloud. Removal from frames is pending until they check in.');
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Could not delete photo.');
    } finally {
      setBusy(false);
    }
  };

  const deleteOrphan = async (frame: Frame, photoId: string) => {
    if (!window.confirm(`Remove ${photoId} from ${frame.name}?`)) return;
    setBusy(true);
    setMessage('');
    try {
      const result = await deleteOrphanPhotoAction(password, frame.id, photoId);
      if (!result.success) throw new Error(result.error || 'Could not queue deletion.');
      await refresh();
      setMessage('Removal queued. The frame will delete the photo when it checks in.');
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Could not queue deletion.');
    } finally { setBusy(false); }
  };

  return (
    <section aria-labelledby="library-heading">
      <div className="section-heading"><h2 id="library-heading">Photo library</h2>
      <p>Manage your cloud photos and see what your frames last reported.</p></div>
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
            <p className="photo-name">{frames.filter((frame) => frame.photoIds.includes(String(photo.id))).map((frame) => frame.name).join(', ') || 'Not reported on a frame'}</p>
            <button type="button" disabled={busy} onClick={() => deletePhoto(photo)} className="delete-button">Delete photo</button>
          </li>)}
        </ul>)}
      {photos && frames.map((frame) => {
        const cloudIds = new Set(photos.map((photo) => String(photo.id)));
        const onlyOnFrame = frame.photoIds.filter((id) => !cloudIds.has(id));
        const pending = frame.pendingIds;
        return <div key={frame.id} className="frame-inventory">
          <h3>{frame.name}</h3>
          <p>{frame.reportedAt ? `Last reported: ${new Date(frame.reportedAt).toLocaleString()} · ${frame.photoIds.length} photos` : 'No inventory reported yet.'}</p>
          {pending.length > 0 && <p>Pending removal: {pending.join(', ')}</p>}
          {onlyOnFrame.length > 0 && <><p>Only on this frame:</p><ul>{onlyOnFrame.map((id) => <li key={id}><span>{id}</span><button type="button" className="delete-button" disabled={busy || pending.includes(id)} onClick={() => deleteOrphan(frame, id)}>{pending.includes(id) ? 'Pending' : 'Delete'}</button></li>)}</ul></>}
        </div>;
      })}
    </section>
  );
}
