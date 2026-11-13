'use server';

import { createClient } from '@/lib/supabase/server';
import { createClient as createAdminClient } from '@supabase/supabase-js';
// test
const ALLOWED_MIME_TYPES = new Set([
  'image/jpeg',
  'image/png',
  'image/jpg',
  'image/bmp',
  'image/x-ms-bmp',
  'image/heic',
  'image/heif'
]);

const EXTENSION_TO_MIME: Record<string, string> = {
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.png': 'image/png',
  '.bmp': 'image/bmp',
  '.heic': 'image/heic',
  '.heif': 'image/heif',
};

async function pingHeartbeat(timeoutMs = 5000): Promise<boolean> {
  const controller = new AbortController();

  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);

  try {
    const response = await fetch('https://photo-frame-image-processor.onrender.com', {
      method: 'GET',
      cache: 'no-store',
      signal: controller.signal
    });

    return response.ok;
  } catch (error) {
    console.log("heartbeat failed or timed out: ", error);
    return false;
  } finally {
    clearTimeout(timeoutId);
  }
}

export async function getSignedUploadUrlAction(password: string, filename: string, contentType: string) {
  try {
    // Verify password server-side
    if (!process.env.UPLOAD_PASSWORD) {
      return { success: false, error: 'Server configuration error: UPLOAD_PASSWORD missing.' };
    }

    if (password !== process.env.UPLOAD_PASSWORD) {
      return { success: false, error: 'Incorrect password.' };
    }

    // Validate and extract extension
    const ext = filename.slice(filename.lastIndexOf('.')).toLowerCase();
    const expectedMime = EXTENSION_TO_MIME[ext];

    if (!expectedMime) {
      return {
        success: false,
        error: 'Unsupported file extension. Allowed: .jpg, .jpeg, .png, .bmp, .heic, .heif',
      };
    }

    const effectiveMime = contentType || expectedMime;
    if (!ALLOWED_MIME_TYPES.has(effectiveMime.toLowerCase())) {
      return { success: false, error: `Unsupported file type: ${contentType}` };
    }

    // make sure render microservice is alive before uploading so that the photo is actually processed
    const microserviceAlive = await pingHeartbeat(90000);

    if (!microserviceAlive) {
      return { 
        success: false,
        error: 'microservice not alive'
      };
    }

    // Sanitize file name
    const sanitizedBaseName = filename
      .replace(/\.[^/.]+$/, '')
      .replace(/[^a-zA-Z0-9-_]/g, '_');
    const filePath = `uploads/${Date.now()}-${sanitizedBaseName}${ext}`;

    // Initialize Supabase client
    const supabase = await createClient();

    // 3. Create signed upload URL
    const { data, error } = await supabase.storage
      .from('photos')
      .createSignedUploadUrl(filePath);

    if (error || !data) {
      return { success: false, error: error?.message || 'Failed to generate upload URL.' };
    }

    return {
      success: true,
      signedUrl: data.signedUrl,
      token: data.token,
      path: data.path,
      contentType: effectiveMime,
    };
  } catch (err: any) {
    return { success: false, error: err?.message || 'An unexpected error occurred.' };
  }
}

function libraryClient(password: string) {
  if (!process.env.UPLOAD_PASSWORD || password !== process.env.UPLOAD_PASSWORD) {
    throw new Error('Incorrect password or missing server configuration.');
  }
  if (!process.env.NEXT_PUBLIC_SUPABASE_URL || !process.env.SUPABASE_SERVICE_ROLE_KEY) {
    throw new Error('Photo library is not configured: SUPABASE_SERVICE_ROLE_KEY is required on the server.');
  }
  return createAdminClient(process.env.NEXT_PUBLIC_SUPABASE_URL, process.env.SUPABASE_SERVICE_ROLE_KEY, {
    auth: { persistSession: false, autoRefreshToken: false },
  });
}

export async function listPhotosAction(password: string) {
  try {
    const supabase = libraryClient(password);
    const photos: { id: string | number; storage_path: string; url: string | null }[] = [];
    const pageSize = 100;
    for (let start = 0; ; start += pageSize) {
      const { data, error } = await supabase.from('images').select('id, storage_path')
        .order('id').range(start, start + pageSize - 1);
      if (error) throw error;
      const rows = data ?? [];
      const signed = await Promise.all(rows.map(async (row) => {
        const { data: image } = await supabase.storage.from('photos').createSignedUrl(row.storage_path, 3600);
        return { id: row.id, storage_path: row.storage_path, url: image?.signedUrl ?? null };
      }));
      photos.push(...signed);
      if (rows.length < pageSize) break;
    }
    return { success: true, photos };
  } catch (error) {
    return { success: false, error: error instanceof Error ? error.message : 'Could not load photos.' };
  }
}

export async function deletePhotoAction(password: string, id: string | number) {
  try {
    const supabase = libraryClient(password);
    const { data: photo, error: lookupError } = await supabase.from('images')
      .select('storage_path').eq('id', id).single();
    if (lookupError || !photo) throw new Error('Photo record not found. Refresh the library and try again.');
    if (typeof photo.storage_path !== 'string' || !photo.storage_path.startsWith('processed/')) {
      throw new Error('The photo has an invalid storage path.');
    }
    const { error: storageError } = await supabase.storage.from('photos').remove([photo.storage_path]);
    if (storageError) throw new Error(`Could not delete the stored photo: ${storageError.message}`);
    const { data: deleted, error: rowError } = await supabase.from('images').delete().eq('id', id).select('id');
    if (rowError || !deleted?.length) {
      throw new Error('The file was deleted, but its database record could not be removed. Try deleting it again.');
    }
    return { success: true };
  } catch (error) {
    return { success: false, error: error instanceof Error ? error.message : 'Could not delete the photo.' };
  }
}
