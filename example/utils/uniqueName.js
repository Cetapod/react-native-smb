// Generate a unique filename by appending " copy", " copy 2", " copy 3", ...
// preserving the file extension. Mirrors macOS Finder behaviour.
//
// `extraReserved` is an optional Set<string> of additional names the caller
// has already chosen but not yet committed to disk (used when generating
// names for several parallel duplicates so they don't collide).

export const generateUniqueName = (fileName, existingNames, extraReserved) => {
  const lastDotIndex = fileName.lastIndexOf('.');
  const baseName = lastDotIndex > 0 ? fileName.substring(0, lastDotIndex) : fileName;
  const extension = lastDotIndex > 0 ? fileName.substring(lastDotIndex) : '';

  // Pattern matches "name copy" or "name copy N"
  const copyPattern = /^(.+?)( copy)( \d+)?$/;
  const match = baseName.match(copyPattern);
  const rootName = baseName.replace(/ copy( \d+)?$/, '');

  const reserved = new Set([
    ...existingNames,
    ...(extraReserved ? Array.from(extraReserved) : []),
  ]);

  // Collect all existing copy numbers that share the root name.
  const copyNumbers = [];
  reserved.forEach((name) => {
    const nameDotIndex = name.lastIndexOf('.');
    const nameBase = nameDotIndex > 0 ? name.substring(0, nameDotIndex) : name;
    const m = nameBase.match(copyPattern);
    if (!m || rootName !== m[1]) return;
    if (!m[3]) {
      copyNumbers.push(1);
    } else {
      copyNumbers.push(parseInt(m[3].trim(), 10));
    }
  });

  let nextNumber = match ? parseInt(match[3]?.trim() || '1', 10) + 1 : 1;
  while (copyNumbers.includes(nextNumber)) {
    nextNumber++;
  }

  const copySuffix = nextNumber === 1 ? 'copy' : `copy ${nextNumber}`;
  return `${rootName} ${copySuffix}${extension}`;
};
