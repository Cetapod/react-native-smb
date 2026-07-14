import type { PathComponents, SmbFileInfo } from './types';

/**
 * Normalize a path to use forward slashes and remove trailing slashes
 * @param path - The path to normalize
 * @returns The normalized path
 */
export function normalizePath(path: string): string {
  const normalized = path.replace(/\\/g, '/');
  return normalized === '/' ? normalized : normalized.replace(/\/+$/, '');
}

/**
 * Split a file path into its components
 * @param path - The full path to split
 * @returns PathComponents containing parent directory, base filename, extension, and full filename
 * 
 * @example
 * splitPath('/folder/document.pdf')
 * // Returns:
 * // {
 * //   parentDirectory: '/folder',
 * //   baseFileName: 'document',
 * //   extension: '.pdf',
 * //   fullFileName: 'document.pdf'
 * // }
 */
export function splitPath(path: string): PathComponents {
  const normalizedPath = normalizePath(path);
  const lastSlashIndex = normalizedPath.lastIndexOf('/');
  
  const parentDirectory = lastSlashIndex >= 0 
    ? normalizedPath.substring(0, lastSlashIndex) || '/'
    : '/';
  
  const fullFileName = lastSlashIndex >= 0 
    ? normalizedPath.substring(lastSlashIndex + 1)
    : normalizedPath;
  
  const lastDotIndex = fullFileName.lastIndexOf('.');
  const hasExtension = lastDotIndex > 0;
  
  const baseFileName = hasExtension 
    ? fullFileName.substring(0, lastDotIndex)
    : fullFileName;
  
  const extension = hasExtension 
    ? fullFileName.substring(lastDotIndex)
    : '';
  
  return {
    parentDirectory,
    baseFileName,
    extension,
    fullFileName,
  };
}

/**
 * Join path components into a full path
 * @param directory - The directory path
 * @param filename - The filename to append
 * @returns The combined path
 * 
 * @example
 * joinPath('/folder', 'file.txt') // '/folder/file.txt'
 * joinPath('/', 'file.txt') // '/file.txt'
 */
export function joinPath(directory: string, filename: string): string {
  if (directory === '/') {
    return `/${filename}`;
  }
  return directory.endsWith('/') 
    ? `${directory}${filename}` 
    : `${directory}/${filename}`;
}

/**
 * Generate a unique copy name for duplicating files/folders
 * Creates names in the format: "name copy.ext", "name copy 2.ext", etc.
 * 
 * @param baseName - The base filename without extension
 * @param extension - The file extension (including the dot)
 * @param existingNames - Array of existing filenames to check against
 * @returns The new unique name
 * 
 * @example
 * generateCopyName('document', '.pdf', ['document.pdf', 'document copy.pdf'])
 * // Returns: 'document copy 2.pdf'
 */
export function generateCopyName(
  baseName: string, 
  extension: string, 
  existingNames: string[]
): string {
  const copyPattern = /^(.+?)( copy)( \d+)?$/;
  const match = baseName.match(copyPattern);
  const rootName = baseName.replace(/ copy( \d+)?$/, '');
  const copyNumbers: number[] = [];
  
  existingNames.forEach((name) => {
    const { baseFileName } = splitPath(name);
    const _match = baseFileName.match(copyPattern);
    
    if (!_match || rootName !== _match[1]) {
      return;
    }
    
    if (!_match[3]) {
      copyNumbers.push(1);
    } else {
      copyNumbers.push(parseInt(_match[3].trim(), 10));
    }
  });
  
  let nextNumber = match ? parseInt(match[3]?.trim() || '1', 10) + 1 : 1;
  while (copyNumbers.includes(nextNumber)) {
    nextNumber++;
  }
  
  const copySuffix = nextNumber === 1 ? 'copy' : `copy ${nextNumber}`;
  return `${rootName} ${copySuffix}${extension}`;
}

/**
 * Get the parent directory of a path
 * @param path - The path to get parent from
 * @returns The parent directory path
 * 
 * @example
 * getParentPath('/folder/subfolder/file.txt') // '/folder/subfolder'
 * getParentPath('/file.txt') // '/'
 */
export function getParentPath(path: string): string {
  const { parentDirectory } = splitPath(path);
  return parentDirectory;
}

/**
 * Get just the filename from a full path
 * @param path - The full path
 * @returns The filename
 * 
 * @example
 * getFileName('/folder/document.pdf') // 'document.pdf'
 */
export function getFileName(path: string): string {
  const { fullFileName } = splitPath(path);
  return fullFileName;
}

/**
 * Format file size in human-readable format
 * @param bytes - Size in bytes
 * @returns Formatted string (e.g., "1.5 MB")
 */
export function formatFileSize(bytes: number): string {
  if (bytes === 0) return '0 Bytes';
  
  const k = 1024;
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  
  return `${parseFloat((bytes / Math.pow(k, i)).toFixed(2))} ${sizes[i]}`;
}

/**
 * Format a timestamp to a readable date string
 * @param timestamp - Unix timestamp in seconds
 * @returns Formatted date string
 */
export function formatDate(timestamp: number): string {
  if (!timestamp) return 'N/A';
  const date = new Date(timestamp * 1000);
  return `${date.toLocaleDateString()} ${date.toLocaleTimeString()}`;
}
