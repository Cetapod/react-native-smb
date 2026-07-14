import { useState } from 'react';
import { Modal, SafeAreaView, ScrollView, Text, TouchableOpacity, View } from 'react-native';

import { getSimplifiedPermission, permissionNames } from '../utils/smbUtils';
import PermissionDetailModal from './acl/PermissionDetailModal';
import { aclStyles as styles } from './acl/styles';

// Map well-known SIDs to friendly names; otherwise show last component.
const sidDisplayName = (sid) => {
  if (!sid) return 'Unknown';
  if (sid === 'S-1-1-0') return 'Everyone';
  if (sid === 'S-1-5-32-544') return 'Administrators';
  if (sid === 'S-1-5-32-545') return 'Users';
  if (sid === 'S-1-5-32-546') return 'Guests';
  const parts = sid.split('-');
  if (parts.length > 3) return `User/Group (${parts[parts.length - 1]})`;
  return sid;
};

const AclViewModal = ({ visible, aclData, selectedFile, loadingAcl, onClose }) => {
  const [expandedSections, setExpandedSections] = useState({ owner: false, group: false, aces: true });
  const [expandedAces, setExpandedAces] = useState({});
  const [expandedPermissions, setExpandedPermissions] = useState({});
  const [permissionDetailModal, setPermissionDetailModal] = useState({
    visible: false,
    permissions: [],
    sid: null,
  });

  const toggleSection = (section) => setExpandedSections((prev) => ({ ...prev, [section]: !prev[section] }));

  const toggleAce = (aceIndex) => setExpandedAces((prev) => ({ ...prev, [aceIndex]: !prev[aceIndex] }));

  const togglePermissionDetail = (aceIndex) => setExpandedPermissions((prev) => ({ ...prev, [aceIndex]: !prev[aceIndex] }));

  const openPermissionDetail = (permissions, sid) => setPermissionDetailModal({ visible: true, permissions: permissions || [], sid });

  const closePermissionDetail = () => setPermissionDetailModal({ visible: false, permissions: [], sid: null });

  const renderPermissions = (permissions, aceIndex = null, sid = null) => {
    if (!permissions) return null;

    const isExpanded = aceIndex !== null ? expandedPermissions[aceIndex] : false;
    const simplified = getSimplifiedPermission(permissions);

    if (simplified && simplified !== 'Custom') {
      return (
        <View>
          <Text style={styles.permissionItem}>{'\u2022 ' + simplified}</Text>
          {aceIndex !== null && (
            <TouchableOpacity
              style={styles.expandPermissionButton}
              onPress={() => togglePermissionDetail(aceIndex)}>
              <Text style={styles.expandPermissionText}>{isExpanded ? 'Hide Details' : 'Show All Permissions'}</Text>
              <Text style={styles.expandPermissionIcon}>{isExpanded ? '\u25B2' : '\u25BC'}</Text>
            </TouchableOpacity>
          )}

          {isExpanded && (
            <View style={styles.detailedPermissionsContainer}>
              <Text style={styles.detailedPermissionsTitle}>All Granted Permissions:</Text>
              {permissions.map((permission, index) => (
                <Text
                  key={index}
                  style={styles.detailedPermissionItem}>
                  {'\u2022 ' + (permissionNames[permission] || permission)}
                </Text>
              ))}
            </View>
          )}
        </View>
      );
    }

    return (
      <View>
        {permissions.map((permission, index) => (
          <Text
            key={index}
            style={styles.permissionItem}>
            {'\u2022 ' + (permissionNames[permission] || permission)}
          </Text>
        ))}
        {aceIndex !== null && (
          <TouchableOpacity
            style={styles.detailButton}
            onPress={() => openPermissionDetail(permissions, sid)}>
            <Text style={styles.detailButtonText}>View Detailed Info</Text>
          </TouchableOpacity>
        )}
      </View>
    );
  };

  const renderAce = (ace, index) => {
    const isExpanded = expandedAces[index];
    const isOwner = aclData?.ownerSid && ace.sid === aclData.ownerSid;
    const isGroup = aclData?.groupSid && ace.sid === aclData.groupSid;
    const simplified = getSimplifiedPermission(ace.mask);

    return (
      <View
        key={index}
        style={styles.aceContainer}>
        <TouchableOpacity
          style={styles.aceHeader}
          onPress={() => toggleAce(index)}>
          <View style={styles.identityContainer}>
            <View style={styles.aceDefaultInfo}>
              <View style={styles.aceHeaderRow}>
                <View style={styles.badgeContainer}>
                  {isOwner && <Text style={styles.ownerBadge}>Owner</Text>}
                  {isGroup && <Text style={styles.groupBadge}>Group</Text>}
                </View>
                <Text style={styles.aceSid}>{sidDisplayName(ace.sid)}</Text>
              </View>
            </View>
            {simplified && <Text style={styles.permissionBadge}>{simplified}</Text>}
            <Text style={styles.expandIcon}>{isExpanded ? '\u25BC' : '\u25B6'}</Text>
          </View>
        </TouchableOpacity>

        {isExpanded && (
          <View style={styles.aceDetails}>
            <Text style={styles.aceSubText}>SID: {ace.sid}</Text>
            <View style={styles.permissionsContainer}>
              <Text style={styles.permissionsTitle}>Permissions:</Text>
              {ace.mask && ace.mask.length > 0 ? renderPermissions(ace.mask, index, ace.sid) : <Text style={styles.permissionItem}>No permissions specified</Text>}
            </View>
          </View>
        )}
      </View>
    );
  };

  return (
    <Modal
      visible={visible}
      animationType="slide"
      onRequestClose={onClose}>
      <SafeAreaView style={styles.container}>
        <View style={styles.header}>
          <Text style={styles.title}>Access Control List</Text>
          <Text style={styles.subtitle}>{selectedFile}</Text>
        </View>

        <ScrollView style={styles.content}>
          {loadingAcl ? (
            <View style={styles.loadingContainer}>
              <Text style={styles.loadingText}>Loading permissions...</Text>
            </View>
          ) : aclData ? (
            <View>
              <Text style={styles.sectionTitle}>Owner</Text>

              {aclData.ownerSid && (
                <View style={styles.ownerContainer}>
                  <TouchableOpacity
                    style={styles.sectionHeader}
                    onPress={() => toggleSection('owner')}>
                    <Text style={styles.ownerText}>Owner</Text>
                    <Text style={styles.expandIcon}>{expandedSections.owner ? '\u25BC' : '\u25B6'}</Text>
                  </TouchableOpacity>
                  {expandedSections.owner && (
                    <View style={styles.detailsContainer}>
                      <Text style={styles.ownerSubText}>SID: {aclData.ownerSid}</Text>
                    </View>
                  )}
                </View>
              )}

              {aclData.groupSid && (
                <View style={styles.groupContainer}>
                  <TouchableOpacity
                    style={styles.sectionHeader}
                    onPress={() => toggleSection('group')}>
                    <Text style={styles.groupText}>Group</Text>
                    <Text style={styles.expandIcon}>{expandedSections.group ? '\u25BC' : '\u25B6'}</Text>
                  </TouchableOpacity>
                  {expandedSections.group && (
                    <View style={styles.detailsContainer}>
                      <Text style={styles.groupSubText}>SID: {aclData.groupSid}</Text>
                    </View>
                  )}
                </View>
              )}

              <View style={styles.acesContainer}>
                <TouchableOpacity
                  style={styles.sectionHeader}
                  onPress={() => toggleSection('aces')}>
                  <Text style={styles.sectionTitle}>Access Control Entries</Text>
                  <Text style={styles.expandIcon}>{expandedSections.aces ? '\u25BC' : '\u25B6'}</Text>
                </TouchableOpacity>
                {expandedSections.aces && (aclData.dacl && aclData.dacl.aces && aclData.dacl.aces.length > 0 ? aclData.dacl.aces.map((ace, index) => renderAce(ace, index)) : <Text style={styles.noDataText}>No access control entries found</Text>)}
              </View>
            </View>
          ) : (
            <View style={styles.noDataContainer}>
              <Text style={styles.noDataText}>No ACL data available</Text>
            </View>
          )}
        </ScrollView>

        <TouchableOpacity
          style={styles.closeButton}
          onPress={onClose}>
          <Text style={styles.closeButtonText}>Close</Text>
        </TouchableOpacity>
      </SafeAreaView>

      <PermissionDetailModal
        visible={permissionDetailModal.visible}
        permissions={permissionDetailModal.permissions}
        sid={permissionDetailModal.sid}
        onClose={closePermissionDetail}
      />
    </Modal>
  );
};

export default AclViewModal;
