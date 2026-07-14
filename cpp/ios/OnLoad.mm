#import <Foundation/Foundation.h>
#include <NitroModules/HybridObjectRegistry.hpp>
#include <ReactNativeSmb/shared/HybridSMB.hpp>

@interface ReactNativeSmbModuleLoader : NSObject
@end

@implementation ReactNativeSmbModuleLoader

+ (void)load {
    using namespace react_native_smb;
    using namespace margelo::nitro;
    
    HybridObjectRegistry::registerHybridObjectConstructor(
        "ReactNativeSmb",
        []() -> std::shared_ptr<HybridObject> {
            return std::make_shared<HybridSMB>();
        }
    );
    
    NSLog(@"ReactNativeSmb module registered");
}

@end
