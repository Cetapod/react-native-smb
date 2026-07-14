// Pure sort/group helpers extracted from FilesScreen. No React, no state.

const getFirstChar = (file) => {
  const firstChar = file.name.charAt(0).toUpperCase();
  return /[A-Z]/.test(firstChar) ? firstChar : '#';
};

export const getFileType = (file) => {
  if (file.isDirectory) return 'Folders';
  const extension = file.name.split('.').pop()?.toLowerCase() || '';
  if (extension) return `${extension.toUpperCase()} Files`;
  return 'Files (No Extension)';
};

const getMonthGroup = (file) => {
  if (!file.modifiedAt) return 'Unknown Date';
  const date = new Date(file.modifiedAt * 1000);
  const year = date.getFullYear();
  const month = date.toLocaleDateString('en-US', { month: 'long' });
  return `${month} ${year}`;
};

const groupFilesByType = (filesList, order = 'asc') => {
  const groups = {};
  filesList.forEach((file) => {
    const type = getFileType(file);
    if (!groups[type]) groups[type] = [];
    groups[type].push(file);
  });

  Object.keys(groups).forEach((type) => {
    groups[type].sort((a, b) => {
      const comparison = a.name.toLowerCase().localeCompare(b.name.toLowerCase());
      return order === 'asc' ? comparison : -comparison;
    });
  });

  const sections = [];
  const sortedTypes = Object.keys(groups).sort((a, b) => {
    if (a === 'Folders') return order === 'asc' ? -1 : 1;
    if (b === 'Folders') return order === 'asc' ? 1 : -1;
    const comparison = a.localeCompare(b);
    return order === 'asc' ? comparison : -comparison;
  });

  sortedTypes.forEach((type) => {
    if (groups[type].length > 0) {
      sections.push({ title: type, data: groups[type], count: groups[type].length });
    }
  });

  return sections;
};

const groupFilesByDate = (filesList, order = 'asc') => {
  const groups = {};
  filesList.forEach((file) => {
    const monthGroup = getMonthGroup(file);
    if (!groups[monthGroup]) groups[monthGroup] = [];
    groups[monthGroup].push(file);
  });

  Object.keys(groups).forEach((monthGroup) => {
    groups[monthGroup].sort((a, b) => {
      const comparison = (b.modifiedAt || 0) - (a.modifiedAt || 0);
      return order === 'asc' ? -comparison : comparison;
    });
  });

  const sortedMonths = Object.keys(groups).sort((a, b) => {
    if (a === 'Unknown Date') return 1;
    if (b === 'Unknown Date') return -1;
    const dateA = groups[a][0]?.modifiedAt || 0;
    const dateB = groups[b][0]?.modifiedAt || 0;
    return order === 'asc' ? dateA - dateB : dateB - dateA;
  });

  return sortedMonths.map((monthGroup) => ({
    title: monthGroup,
    data: groups[monthGroup],
    count: groups[monthGroup].length,
  }));
};

const groupFilesByFirstChar = (filesList, order = 'asc') => {
  const groups = {};
  filesList.forEach((file) => {
    const charGroup = getFirstChar(file);
    if (!groups[charGroup]) groups[charGroup] = [];
    groups[charGroup].push(file);
  });

  Object.keys(groups).forEach((charGroup) => {
    groups[charGroup].sort((a, b) => {
      const comparison = a.name.toLowerCase().localeCompare(b.name.toLowerCase());
      return order === 'asc' ? comparison : -comparison;
    });
  });

  const sections = [];
  for (let char = 'A'.charCodeAt(0); char <= 'Z'.charCodeAt(0); char++) {
    const key = String.fromCharCode(char);
    if (groups[key]) {
      sections.push({ title: key, data: groups[key], count: groups[key].length });
    }
  }
  if (groups['#']) {
    sections.push({ title: '#', data: groups['#'], count: groups['#'].length });
  }
  return order === 'asc' ? sections : sections.reverse();
};

// Top-level sort entry point. Returns either:
//   { isGrouped: true, sections: [...] }  -- for SectionList
//   { isGrouped: false, data: [...] }     -- for FlatList
export const sortFiles = (filesList, sortBy, sortOrder) => {
  const safeFiles = Array.isArray(filesList) ? filesList : [];
  if (sortBy === 'type') {
    return { isGrouped: true, sections: groupFilesByType(safeFiles, sortOrder) };
  }
  if (sortBy === 'date') {
    return { isGrouped: true, sections: groupFilesByDate(safeFiles, sortOrder) };
  }
  if (sortBy === 'name') {
    return { isGrouped: true, sections: groupFilesByFirstChar(safeFiles, sortOrder) };
  }

  const sorted = [...safeFiles].sort((a, b) => {
    if (a.isDirectory && !b.isDirectory) return -1;
    if (!a.isDirectory && b.isDirectory) return 1;

    let comparison = 0;
    switch (sortBy) {
      case 'size':
        comparison = (a.size || 0) - (b.size || 0);
        break;
      default:
        comparison = a.name.toLowerCase().localeCompare(b.name.toLowerCase());
    }
    return sortOrder === 'asc' ? comparison : -comparison;
  });

  return { isGrouped: false, data: sorted };
};

export const filterFiles = (filesList, query) => {
  const safeFiles = Array.isArray(filesList) ? filesList : [];
  if (!query.trim()) return safeFiles;
  const lowerQuery = query.toLowerCase();
  return safeFiles.filter((file) => file.name.toLowerCase().includes(lowerQuery));
};
