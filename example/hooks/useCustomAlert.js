import { useState } from 'react';

export const useCustomAlert = () => {
  const [alertConfig, setAlertConfig] = useState({
    visible: false,
    type: 'alert',
    title: '',
    message: '',
    placeholder: '',
    buttons: [],
    onInputSubmit: null,
  });

  const showAlert = (title, message, buttons = []) => {
    setAlertConfig({
      visible: true,
      type: 'alert',
      title,
      message,
      placeholder: '',
      buttons: buttons.length > 0 ? buttons : [{ text: 'OK' }],
      onInputSubmit: null,
    });
  };

  const showPrompt = (title, message, callbackOrButtons, type = 'plain-text', defaultValue = '', buttons = []) => {
    // Handle different parameter combinations similar to Alert.prompt
    let onInputSubmit = null;
    let promptButtons = [];

    if (typeof callbackOrButtons === 'function') {
      // Alert.prompt(title, message, callback)
      onInputSubmit = callbackOrButtons;
      promptButtons = [
        { text: 'Cancel', onPress: () => onInputSubmit(null) },
        { text: 'OK', onPress: () => {} }, // Will be handled by onInputSubmit
      ];
    } else if (Array.isArray(callbackOrButtons)) {
      // Alert.prompt(title, message, buttons)
      promptButtons = callbackOrButtons;
    } else {
      // Default buttons
      promptButtons = [{ text: 'Cancel' }, { text: 'OK' }];
    }

    setAlertConfig({
      visible: true,
      type: 'prompt',
      title,
      message,
      placeholder: defaultValue || 'Enter text',
      buttons: promptButtons,
      onInputSubmit,
    });
  };

  const hideAlert = () => {
    setAlertConfig((prev) => ({
      ...prev,
      visible: false,
    }));
  };

  return {
    alertConfig,
    showAlert,
    showPrompt,
    hideAlert,
  };
};
