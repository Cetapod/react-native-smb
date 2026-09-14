import { SmbError } from '@cetapod/react-native-smb';

const DEFAULT_FRIENDLY = {
  title: 'Error',
  message: 'An unknown error occurred',
};

const errorCode = (error) => error?.code;

const isError = (code) => (error) => errorCode(error) === code;

export const isAccessDeniedError = isError(SmbError.AccessDenied);

const STATIC_FRIENDLY = {
  [SmbError.AuthenticationFailed]: {
    title: 'Authentication Failed',
    message:
      'The username or password is incorrect.\n\nPlease check:\n• Username is spelled correctly\n• Password is correct\n• Caps Lock is off',
  },
  [SmbError.NotConnected]: {
    title: 'Not Connected',
    message:
      'The SMB connection is closed or unavailable.\n\nPlease check:\n• You are connected to the server\n• The connection was not interrupted\n• Try reconnecting before retrying the operation',
  },
  [SmbError.ConnectionRefused]: {
    title: 'Connection Refused',
    message:
      'The server refused the connection.\n\nPlease check:\n• Server address is correct\n• Server is online and accessible\n• Firewall settings allow SMB (port 445)',
  },
  [SmbError.TimedOut]: {
    title: 'Timed Out',
    message:
      'The operation made no progress before its timeout.\n\nPlease check:\n• Network connection is stable\n• Server is responding\n• Try again with a smaller operation or longer timeout',
  },
};

const CONTEXTUAL_FRIENDLY = {
  [SmbError.AccessDenied]: (context) => {
    const ctx = context ? ` accessing ${context}` : '';
    return {
      title: 'Access Denied',
      message: `You don't have permission${ctx}.\n\nPossible solutions:\n• Check if your user account has the required permissions\n• Try using an administrator account\n• Contact your system administrator for access`,
    };
  },
  [SmbError.NotFound]: (context) => ({
    title: 'Not Found',
    message: `The requested ${context || 'item'} was not found.\n\nIt may have been moved or deleted.`,
  }),
};

const getFriendlyError = (error, context = '') => {
  if (!error) return DEFAULT_FRIENDLY;

  const code = errorCode(error);
  const contextual = CONTEXTUAL_FRIENDLY[code];
  if (contextual) return contextual(context);

  const staticMessage = STATIC_FRIENDLY[code];
  if (staticMessage) return staticMessage;

  return {
    title: 'Operation Failed',
    message: error.message || String(error),
  };
};

export const handleSMBError = (error, operation, context, showAlert) => {
  console.error(`[SMB Error] ${operation}:`, {
    code: error?.code ?? SmbError.Unknown,
    message: error?.message || String(error),
    context,
    timestamp: new Date().toISOString(),
  });

  const friendly = getFriendlyError(error, context);
  if (showAlert) showAlert(friendly.title, friendly.message);
  return friendly;
};
