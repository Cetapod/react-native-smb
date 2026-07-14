import { joinPath } from '@cetapod/react-native-smb';

export const convertUriToPath = (uri) => {
  if (!uri) {
    throw new Error('URI cannot be null or undefined');
  }

  if (uri.startsWith('file://')) {
    return decodeURIComponent(uri.replace('file://', ''));
  } else if (uri.startsWith('content://')) {
    throw new Error('Content URIs are not supported. Please use file URIs.');
  }

  return uri;
};

export const buildFilePath = (currentPath, fileName) => {
  return joinPath(currentPath, fileName);
};

export const buildFilePathWithoutLeadingSlash = (currentPath, fileName) => {
  const fullPath = buildFilePath(currentPath, fileName);
  return fullPath.startsWith('/') ? fullPath.substring(1) : fullPath;
};
