// Centralized design tokens for the example app.
//
// Colors are sampled from existing screens/components and consolidated
// here. Components may import what they need; legacy inline hex literals
// are kept until each component is updated.

export const colors = {
  // Primary blue palette
  primary: '#4A90E2',
  primaryDark: '#1E3A5F',
  primaryMedium: '#4A6B8A',
  primaryLight: '#7BA8D1',
  primarySoft: '#B8D4F1',
  primaryAccent: '#6B8CAE',

  // iOS system blue (used for indicators)
  systemBlue: '#007AFF',

  // Backgrounds
  background: '#F0F8FF', // Alice Blue
  backgroundLight: '#F8FBFF',
  backgroundLighter: '#E6F2FF',
  backgroundLightest: '#E8F4FD',
  card: '#FFFFFF',
  divider: '#D6E9F7',

  // Folder picker neutrals
  neutral50: '#F9FAFB',
  neutral100: '#F3F4F6',
  neutral200: '#E5E7EB',
  neutral300: '#D1D5DB',
  neutral400: '#9CA3AF',
  neutral500: '#6B7280',
  neutral900: '#111827',
  pickerPrimary: '#3B82F6',
  pickerHighlight: '#EFF6FF',

  // Generic grays
  gray50: '#F5F5F5',
  gray200: '#E0E0E0',
  gray300: '#C0C0C0',

  // Status
  success: '#4CAF50',
  danger: '#FF5C5C',

  // Pure
  white: '#FFFFFF',
  black: '#000000',

  // Overlays
  overlay: 'rgba(0, 0, 0, 0.5)',
  overlayLight: 'rgba(0, 0, 0, 0.4)',
  overlayNeutral: 'rgba(128, 128, 128, 0.4)',

  // Aliases for legacy text helpers
  textPrimary: '#1E3A5F',
  textSecondary: '#4A6B8A',
  textMuted: '#7BA8D1',
};

export const spacing = {
  xs: 4,
  sm: 8,
  md: 12,
  lg: 16,
  xl: 20,
  xxl: 24,
  xxxl: 32,
};

export const radii = {
  sm: 6,
  md: 10,
  lg: 12,
  xl: 16,
  pill: 999,
};

export const shadow = {
  sm: {
    shadowColor: colors.primary,
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.08,
    shadowRadius: 4,
    elevation: 1,
  },
  md: {
    shadowColor: colors.primary,
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.12,
    shadowRadius: 8,
    elevation: 3,
  },
};

export default { colors, spacing, radii, shadow };
