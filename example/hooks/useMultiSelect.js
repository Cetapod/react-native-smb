// useMultiSelect: lightweight selection state for the files screen.
//
// Items are tracked by a string key (use buildFilePath result).
// Returns helpers + the Set itself so consumers can render selection UI.

import { useCallback, useState } from 'react';

export const useMultiSelect = () => {
  const [selectionMode, setSelectionMode] = useState(false);
  const [selected, setSelected] = useState(() => new Set());

  const enterSelection = useCallback((key) => {
    setSelectionMode(true);
    if (key) setSelected(new Set([key]));
  }, []);

  const exit = useCallback(() => {
    setSelectionMode(false);
    setSelected(new Set());
  }, []);

  const toggle = useCallback((key) => {
    setSelected((prev) => {
      const next = new Set(prev);
      if (next.has(key)) next.delete(key);
      else next.add(key);
      return next;
    });
  }, []);

  const isSelected = useCallback((key) => selected.has(key), [selected]);

  const selectAll = useCallback((keys) => {
    setSelected(new Set(keys));
  }, []);

  // Toggle between "all selected" and "none selected" for the given key list.
  // If every key in `keys` is currently selected we clear; otherwise we add all.
  const toggleAll = useCallback((keys) => {
    setSelected((prev) => {
      const allSelected = keys.length > 0 && keys.every((k) => prev.has(k));
      if (allSelected) return new Set();
      return new Set(keys);
    });
  }, []);

  // Explicit entry into selection mode without preselecting anything (used by
  // the header "Select" button).
  const enterEmpty = useCallback(() => {
    setSelectionMode(true);
    setSelected(new Set());
  }, []);

  return { selectionMode, selected, enterSelection, enterEmpty, exit, toggle, isSelected, selectAll, toggleAll };
};

export default useMultiSelect;
