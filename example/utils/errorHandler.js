const lower = (error) => (error && error.message ? String(error.message).toLowerCase() : '');

export const isAccessDeniedError = (error) => {
  const m = lower(error);
  if (!m) return false;
  return m.includes('access_denied') || m.includes('access denied') || m.includes('status_access_denied') || m.includes('0xc0000022');
};

const isConnectionError = (error) => {
  const m = lower(error);
  if (!m) return false;
  return m.includes('connection') || m.includes('connect') || m.includes('network') || m.includes('timeout');
};

const isAuthenticationError = (error) => {
  const m = lower(error);
  if (!m) return false;
  return m.includes('logon failure') || m.includes('invalid username') || m.includes('invalid password') || m.includes('authentication') || m.includes('0xc000006d') || m.includes('0xc000006e');
};

const friendly = (error, context = '') => {
  if (!error) return { title: 'Error', message: 'An unknown error occurred' };
  const message = error.message || String(error);

  if (isAccessDeniedError(error)) {
    const ctx = context ? ` accessing ${context}` : '';
    return {
      title: 'Access Denied',
      message: `You don't have permission${ctx}.\n\nPossible solutions:\n• Check if your user account has the required permissions\n• Try using an administrator account\n• Contact your system administrator for access`,
    };
  }

  if (isAuthenticationError(error)) {
    return {
      title: 'Authentication Failed',
      message: 'The username or password is incorrect.\n\nPlease check:\n• Username is spelled correctly\n• Password is correct\n• Caps Lock is off',
    };
  }

  if (isConnectionError(error)) {
    return {
      title: 'Connection Failed',
      message: 'Cannot connect to the server.\n\nPlease check:\n• Server address is correct\n• Server is online and accessible\n• Network connection is working\n• Firewall settings allow SMB (port 445)',
    };
  }

  if (message.includes('IPC$')) {
    return {
      title: 'Share Enumeration Failed',
      message: 'Cannot list available shares.\n\nThis usually means:\n• Your account lacks administrative privileges\n• The server restricts IPC$ access\n\nTry connecting directly to a known share name instead.',
    };
  }

  if (message.includes('not found') || message.includes('0xc0000034')) {
    return {
      title: 'Not Found',
      message: `The requested ${context || 'item'} was not found.\n\nIt may have been moved or deleted.`,
    };
  }

  return { title: 'Operation Failed', message };
};

export const handleSMBError = (error, operation, context, showAlert) => {
  console.error(`[SMB Error] ${operation}:`, {
    message: error?.message || String(error),
    context,
    timestamp: new Date().toISOString(),
  });
  const f = friendly(error, context);
  if (showAlert) showAlert(f.title, f.message);
  return f;
};
