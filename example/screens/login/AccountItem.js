import { useCallback, useRef } from 'react';
import { Animated, PanResponder, Text, TouchableOpacity, View } from 'react-native';

import { loginStyles as styles, MAX_REVEAL, SWIPE_THRESHOLD } from './styles';

// Swipe-to-reveal row for a single saved account.
// openRowRef is a shared ref owned by the parent so that opening one row
// closes any other already-open row.

const AccountItem = ({ account, index, isFirst, isLoading, onSelect, onRemove, openRowRef }) => {
  const translateX = useRef(new Animated.Value(0)).current;
  const isOpen = useRef(false);
  const isDragging = useRef(false);

  const closeRow = useCallback(() => {
    if (isOpen.current) {
      Animated.timing(translateX, {
        toValue: 0,
        duration: 200,
        useNativeDriver: true,
      }).start(() => {
        isOpen.current = false;
      });
    }
  }, [translateX]);

  const registerAsOpen = useCallback(() => {
    if (openRowRef.current && openRowRef.current !== closeRow) {
      openRowRef.current();
    }
    openRowRef.current = closeRow;
    isOpen.current = true;
  }, [closeRow, openRowRef]);

  const panResponder = useRef(
    PanResponder.create({
      onStartShouldSetPanResponder: () => true,
      onMoveShouldSetPanResponder: (_, gestureState) => {
        const { dx, dy } = gestureState;
        return Math.abs(dx) > 10 && Math.abs(dx) > Math.abs(dy) * 2;
      },
      onPanResponderGrant: () => {
        isDragging.current = false;
      },
      onPanResponderMove: (_, gestureState) => {
        const dx = gestureState.dx;
        if (dx < 0) {
          isDragging.current = true;
          const clamped = Math.max(dx, -MAX_REVEAL - 10);
          translateX.setValue(clamped);
        } else if (isOpen.current && dx > 0) {
          const clamped = Math.min(dx, 0);
          translateX.setValue(-MAX_REVEAL + clamped);
        }
      },
      onPanResponderRelease: (_, gestureState) => {
        const dx = gestureState.dx;
        const velocityX = gestureState.vx;

        let targetValue = 0;
        if (dx < -SWIPE_THRESHOLD || velocityX < -0.5) {
          targetValue = -MAX_REVEAL;
          registerAsOpen();
        } else if (isOpen.current && (dx > 20 || velocityX > 0.5)) {
          targetValue = 0;
          isOpen.current = false;
          if (openRowRef.current === closeRow) {
            openRowRef.current = null;
          }
        } else if (isOpen.current) {
          targetValue = -MAX_REVEAL;
        }

        Animated.spring(translateX, {
          toValue: targetValue,
          useNativeDriver: true,
          tension: 300,
          friction: 30,
        }).start(() => {
          setTimeout(() => {
            isDragging.current = false;
          }, 100);
        });
      },
    }),
  ).current;

  const handlePress = () => {
    if (isLoading || isDragging.current) return;
    onSelect(account);
  };

  const handleRemove = () => {
    Animated.timing(translateX, {
      toValue: 0,
      duration: 200,
      useNativeDriver: true,
    }).start(() => {
      isOpen.current = false;
      if (openRowRef.current === closeRow) {
        openRowRef.current = null;
      }
      onRemove(account);
    });
  };

  const removeOpacity = translateX.interpolate({
    inputRange: [-MAX_REVEAL, -20, 0],
    outputRange: [1, 0.1, 0],
    extrapolate: 'clamp',
  });

  const removeScale = translateX.interpolate({
    inputRange: [-MAX_REVEAL, -40, 0],
    outputRange: [1, 0.8, 0.6],
    extrapolate: 'clamp',
  });

  return (
    <View
      key={index}
      style={[styles.savedAccountItem, isFirst && styles.primaryAccountItem]}>
      <Animated.View
        style={[styles.removeButtonContainer, { opacity: removeOpacity, transform: [{ scale: removeScale }] }]}
        pointerEvents="box-none">
        <TouchableOpacity
          style={styles.removeButton}
          onPress={handleRemove}
          activeOpacity={0.7}
          hitSlop={{ top: 10, bottom: 10, left: 10, right: 10 }}>
          <Text style={styles.removeButtonText}>Remove</Text>
        </TouchableOpacity>
      </Animated.View>

      <Animated.View
        {...panResponder.panHandlers}
        style={[styles.accountContent, { transform: [{ translateX }] }]}>
        <TouchableOpacity
          style={{ flex: 1 }}
          onPress={handlePress}
          disabled={isLoading}
          activeOpacity={0.7}>
          <View style={styles.accountContentInner}>
            <View style={styles.accountAvatar}>
              <Text style={styles.accountAvatarText}>{account.username.charAt(0).toUpperCase()}</Text>
            </View>
            <View style={styles.accountInfo}>
              <View style={styles.accountHeader}>
                <Text style={styles.accountUsername}>{account.username}</Text>
                {isFirst && <Text style={styles.recentBadge}>Recent</Text>}
              </View>
              <Text
                style={styles.accountServer}
                numberOfLines={1}>
                {account.serverUrl}
              </Text>
              {account.lastUsed && <Text style={styles.lastUsed}>Last used: {new Date(account.lastUsed).toLocaleDateString()}</Text>}
            </View>
          </View>
        </TouchableOpacity>
        <View style={styles.quickLoginButton}>
          <Text style={styles.quickLoginIcon}>{'\u2192'}</Text>
        </View>
      </Animated.View>
    </View>
  );
};

export default AccountItem;
