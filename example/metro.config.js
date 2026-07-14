const { getDefaultConfig } = require('expo/metro-config');
const path = require('path');

const projectRoot = __dirname;
const workspaceRoot = path.resolve(projectRoot, '..');

const config = getDefaultConfig(projectRoot);

config.watchFolders = [workspaceRoot];

const { peerDependencies = {} } = require(path.join(workspaceRoot, 'package.json'));
const shared = Object.keys(peerDependencies);

const isShared = (name) =>
  shared.some((pkg) => name === pkg || name.startsWith(`${pkg}/`));

config.resolver.resolveRequest = (context, moduleName, platform) => {
  const target = isShared(moduleName)
    ? path.join(projectRoot, 'node_modules', moduleName)
    : moduleName;

  return context.resolveRequest(context, target, platform);
};

module.exports = config;
