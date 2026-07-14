import React, { useState, useRef } from 'react';
import LinearGradient from 'react-native-linear-gradient';
import { Modal, StyleSheet, Text, TextInput, TouchableOpacity, View, ScrollView, Dimensions } from 'react-native';

const CustomAlert = ({ visible, type, title, message, placeholder, buttons, onClose, onInputSubmit }) => {
  const [inputValue, setInputValue] = useState('');

  // Constrain message box height so very long messages don't push content off-screen
  const windowHeight = Dimensions.get('window').height;
  const maxMessageHeight = Math.min(480, windowHeight * 0.45); // cap for very tall screens

  // Scroll tracking for fade overlays
  const [showTopFade, setShowTopFade] = useState(false);
  const [showBottomFade, setShowBottomFade] = useState(false);

  // refs declared below once
  const contentHeightRef = useRef(0);
  const visibleHeightRef = useRef(0);

  const FADE_THRESHOLD = 6;

  const updateFades = (scrollY = 0) => {
    const contentH = contentHeightRef.current;
    const visibleH = visibleHeightRef.current;

    if (!contentH || !visibleH || contentH <= visibleH + 1) {
      setShowTopFade(false);
      setShowBottomFade(false);
      return;
    }

    setShowTopFade(scrollY > FADE_THRESHOLD);
    setShowBottomFade(scrollY + visibleH < contentH - FADE_THRESHOLD);
  };

  const handleButtonPress = (buttonIndex, action) => {
    if (type === 'prompt' && buttonIndex === 1 && onInputSubmit) {
      // For prompt type, pass input value to onInputSubmit for the positive button
      onInputSubmit(inputValue);
    } else if (action) {
      action();
    }

    // Clear input and close
    setInputValue('');
    onClose();
  };

  const renderButtons = () => {
    if (!buttons || buttons.length === 0) {
      // Default OK button
      return (
        <TouchableOpacity
          style={[styles.button, styles.singleButton]}
          onPress={() => handleButtonPress(0)}>
          <Text style={styles.buttonText}>OK</Text>
        </TouchableOpacity>
      );
    }

    if (buttons.length === 1) {
      return (
        <TouchableOpacity
          style={[styles.button, styles.singleButton]}
          onPress={() => handleButtonPress(0, buttons[0].onPress)}>
          <Text style={styles.buttonText}>{buttons[0].text}</Text>
        </TouchableOpacity>
      );
    }

    if (buttons.length === 2) {
      // Two buttons (Cancel/OK or similar)
      return (
        <View style={styles.buttonsContainer}>
          <TouchableOpacity
            style={[styles.button, styles.cancelButton]}
            onPress={() => handleButtonPress(0, buttons[0].onPress)}>
            <Text style={styles.cancelButtonText}>{buttons[0].text}</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={[styles.button, styles.confirmButton]}
            onPress={() => handleButtonPress(1, buttons[1].onPress)}>
            <Text style={styles.confirmButtonText}>{buttons[1].text}</Text>
          </TouchableOpacity>
        </View>
      );
    }

    // Three or more buttons - stack vertically
    return (
      <View style={styles.verticalButtonsContainer}>
        {buttons.map((button, index) => (
          <TouchableOpacity
            key={index}
            style={[styles.button, styles.verticalButton, index === buttons.length - 1 && styles.lastVerticalButton]}
            onPress={() => handleButtonPress(index, button.onPress)}>
            <Text style={[index === buttons.length - 1 ? styles.buttonText : styles.cancelButtonText]}>{button.text}</Text>
          </TouchableOpacity>
        ))}
      </View>
    );
  };

  return (
    <Modal
      visible={visible}
      transparent={true}
      animationType="fade"
      onRequestClose={onClose}>
      <View style={styles.overlay}>
        <View style={styles.container}>
          {title && <Text style={styles.title}>{title}</Text>}
          {message && (
            <View style={[styles.messageWrapper, { maxHeight: maxMessageHeight }]}>
              <ScrollView
                style={styles.messageContainer}
                contentContainerStyle={{ paddingVertical: 4 }}
                showsVerticalScrollIndicator={true}
                onLayout={(e) => {
                  visibleHeightRef.current = e.nativeEvent.layout.height;
                  updateFades(0);
                }}
                onContentSizeChange={(w, h) => {
                  contentHeightRef.current = h;
                  updateFades(0);
                }}
                onScroll={(e) => {
                  const y = e.nativeEvent.contentOffset.y;
                  updateFades(y);
                }}
                scrollEventThrottle={50}>
                <Text style={styles.message}>{message}</Text>
              </ScrollView>

              {/* Top fade */}
              {showTopFade && <Fade top />}

              {/* Bottom fade */}
              {showBottomFade && <Fade />}
            </View>
          )}

          {type === 'prompt' && (
            <TextInput
              style={styles.input}
              placeholder={placeholder || 'Enter text'}
              value={inputValue}
              onChangeText={setInputValue}
              autoFocus={true}
              selectTextOnFocus={true}
            />
          )}

          {renderButtons()}
        </View>
      </View>
    </Modal>
  );
};

const Fade = ({ top }) => (
  <LinearGradient
    colors={top ? ['rgba(240,248,255,1)', 'rgba(240,248,255,0)'] : ['rgba(240,248,255,0)', 'rgba(240,248,255,1)']}
    style={[styles.fade, top ? styles.topFade : styles.bottomFade]}
  />
);

const styles = StyleSheet.create({
  overlay: {
    flex: 1,
    backgroundColor: 'rgba(128, 128, 128, 0.4)', // Neutral gray overlay
    justifyContent: 'center',
    alignItems: 'center',
  },
  container: {
    width: '85%',
    maxWidth: 400,
    backgroundColor: '#F0F8FF', // Alice Blue
    borderRadius: 12,
    padding: 20,
    borderWidth: 1,
    borderColor: '#87CEEB', // Sky Blue border
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 6 },
    shadowOpacity: 0.15,
    shadowRadius: 12,
    elevation: 8,
  },
  title: {
    fontSize: 18,
    fontWeight: '600',
    color: '#191970', // Midnight Blue
    textAlign: 'center',
    marginBottom: 12,
  },
  message: {
    fontSize: 16,
    color: '#4682B4', // Steel Blue
    textAlign: 'center',
    lineHeight: 22,
  },
  messageContainer: {
    flexGrow: 1,
  },
  messageWrapper: {
    position: 'relative',
    width: '100%',
    overflow: 'hidden',
    marginBottom: 20,
  },
  fade: {
    position: 'absolute',
    left: 0,
    right: 0,
    height: 20,
  },
  topFade: {
    top: 0,
  },
  bottomFade: {
    bottom: 0,
  },
  input: {
    height: 44,
    backgroundColor: '#FFFFFF', // White
    borderRadius: 8,
    paddingHorizontal: 12,
    fontSize: 16,
    borderWidth: 1,
    borderColor: '#87CEEB', // Sky Blue border
    color: '#191970', // Midnight Blue text
    marginBottom: 20,
  },
  buttonsContainer: {
    flexDirection: 'row',
    gap: 12,
  },
  verticalButtonsContainer: {
    flexDirection: 'column',
    gap: 10,
  },
  verticalButton: {
    backgroundColor: '#F0F8FF', // Light blue background
    borderWidth: 1,
    borderColor: '#87CEEB', // Sky Blue border
    width: '100%',
  },
  lastVerticalButton: {
    backgroundColor: '#4169E1', // Royal Blue for the last (primary) button
    borderWidth: 0,
  },
  button: {
    height: 44,
    borderRadius: 8,
    justifyContent: 'center',
    alignItems: 'center',
  },
  singleButton: {
    backgroundColor: '#4169E1', // Royal Blue
    width: '100%',
  },
  cancelButton: {
    flex: 1,
    backgroundColor: '#F0F8FF', // Light blue background
    borderWidth: 1,
    borderColor: '#87CEEB', // Sky Blue border
  },
  confirmButton: {
    flex: 1,
    backgroundColor: '#4169E1', // Royal Blue
  },
  buttonText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#FFFFFF', // White text
  },
  cancelButtonText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#4682B4', // Steel Blue text
  },
  confirmButtonText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#FFFFFF', // White text
  },
});

export default CustomAlert;
