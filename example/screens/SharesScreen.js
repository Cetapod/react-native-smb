import React from 'react';
import { View, Text, TouchableOpacity, StyleSheet, SafeAreaView, FlatList, ActivityIndicator } from 'react-native';

const SharesScreen = ({ shares, onSelectShare, isLoading, onBack }) => {
  const renderShareItem = ({ item }) => (
    <TouchableOpacity
      style={styles.shareItem}
      onPress={() => onSelectShare(item)}
      disabled={isLoading}>
      <View style={styles.shareInfo}>
        <Text style={styles.shareName}>{item.name || item}</Text>
        <Text style={styles.shareType}>SMB Share</Text>
      </View>
      <Text style={styles.chevron}>›</Text>
    </TouchableOpacity>
  );

  return (
    <SafeAreaView style={styles.container}>
      {/* Header */}
      <View style={styles.header}>
        <TouchableOpacity
          style={styles.backButton}
          onPress={onBack}>
          <Text style={styles.backButtonText}>‹ Back</Text>
        </TouchableOpacity>
        <Text style={styles.title}>Select Share</Text>
        <View style={styles.placeholder} />
      </View>

      {/* Content */}
      <View style={styles.content}>
        {isLoading ? (
          <View style={styles.loadingContainer}>
            <ActivityIndicator
              size="large"
              color="#007AFF"
            />
            <Text style={styles.loadingText}>Loading shares...</Text>
          </View>
        ) : shares.length === 0 ? (
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyTitle}>No Shares Found</Text>
            <Text style={styles.emptySubtitle}>No SMB shares were found on this server</Text>
          </View>
        ) : (
          <FlatList
            data={shares}
            renderItem={renderShareItem}
            keyExtractor={(item, index) => index.toString()}
            showsVerticalScrollIndicator={false}
            contentContainerStyle={styles.listContainer}
          />
        )}
      </View>
    </SafeAreaView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#F0F8FF', // Light blue background (Alice Blue)
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 16,
    paddingVertical: 16,
    backgroundColor: '#FFFFFF', // Pure white
    borderBottomWidth: 1,
    borderBottomColor: '#B8D4F1', // Soft blue border
    shadowColor: '#4A90E2',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.08,
    shadowRadius: 4,
    elevation: 2,
  },
  backButton: {
    padding: 8,
  },
  backButtonText: {
    fontSize: 17,
    color: '#4A90E2', // Primary blue
    fontWeight: '500',
  },
  title: {
    fontSize: 17,
    fontWeight: '600',
    color: '#1E3A5F', // Deep blue text
  },
  placeholder: {
    width: 60,
  },
  content: {
    flex: 1,
  },
  loadingContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    gap: 16,
  },
  loadingText: {
    fontSize: 16,
    color: '#4A6B8A', // Medium blue text
  },
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingHorizontal: 32,
  },
  emptyIcon: {
    fontSize: 64,
    marginBottom: 16,
    opacity: 0.6,
  },
  emptyTitle: {
    fontSize: 20,
    fontWeight: '600',
    color: '#1E3A5F', // Deep blue text
    marginBottom: 8,
  },
  emptySubtitle: {
    fontSize: 16,
    color: '#4A6B8A', // Medium blue text
    textAlign: 'center',
    lineHeight: 22,
  },
  listContainer: {
    padding: 16,
    gap: 1,
  },
  shareItem: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#FFFFFF', // Pure white
    paddingHorizontal: 16,
    paddingVertical: 12,
    borderRadius: 10,
    marginBottom: 1,
    borderWidth: 1,
    borderColor: '#E6F2FF', // Very light blue border
    shadowColor: '#4A90E2',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.05,
    shadowRadius: 3,
    elevation: 1,
  },
  shareIcon: {
    width: 40,
    height: 40,
    backgroundColor: '#B8D4F1', // Soft blue accent
    borderRadius: 8,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
  },
  shareIconText: {
    fontSize: 20,
  },
  shareInfo: {
    flex: 1,
  },
  shareName: {
    fontSize: 16,
    fontWeight: '500',
    color: '#1E3A5F', // Deep blue text
    marginBottom: 2,
  },
  shareType: {
    fontSize: 14,
    color: '#4A6B8A', // Medium blue text
  },
  chevron: {
    fontSize: 20,
    color: '#7BA8D1', // Light blue chevron
    fontWeight: '300',
  },
});

export default SharesScreen;
