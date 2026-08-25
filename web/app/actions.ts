'use server';

import { createClient } from '@/lib/supabase/server';

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