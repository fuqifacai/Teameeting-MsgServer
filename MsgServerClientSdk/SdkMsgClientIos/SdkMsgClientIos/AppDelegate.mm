//
//  AppDelegate.m
//  SdkMsgClientIos
//
//  Created by hp on 1/31/16.
//  Copyright © 2016 Dync. All rights reserved.
//

#import "AppDelegate.h"
#import "ViewController.h"

@interface AppDelegate ()

@end

@implementation AppDelegate


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    ViewController *view = [ViewController new];
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    [self.window setRootViewController:[[UINavigationController alloc] initWithRootViewController:view]];
    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 80000
    if ([[UIApplication sharedApplication] respondsToSelector:@selector(registerUserNotificationSettings:)])
    {
        UIUserNotificationSettings *settings = [UIUserNotificationSettings settingsForTypes:(UIUserNotificationTypeBadge|UIUserNotificationTypeAlert|UIUserNotificationTypeSound) categories:nil];
        [[UIApplication sharedApplication] registerUserNotificationSettings:settings];
        
    } else
#endif
    {
        /// 去除warning
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [[UIApplication sharedApplication] registerForRemoteNotificationTypes:
         (UIRemoteNotificationTypeBadge | UIRemoteNotificationTypeSound | UIRemoteNotificationTypeAlert)];
#pragma clang diagnostic pop
    }
    
    NSDictionary *pushInfo = [launchOptions objectForKey:UIApplicationLaunchOptionsRemoteNotificationKey];
    NSLog(@"======>>>pushInfo == %@",pushInfo);
    NSString *message = [pushInfo objectForKey:@"push"];
    NSLog(@"======>>>pushInfo---notification message:%@", message);
    
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"notification" message:message delegate:self cancelButtonTitle:@"cancel" otherButtonTitles:@"other", nil];
    
    [alert show];
    return YES;
}
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 80000
- (void)application:(UIApplication *)application didRegisterUserNotificationSettings:(UIUserNotificationSettings *)notificationSettings
{
    if ([[UIApplication sharedApplication] respondsToSelector:@selector(registerForRemoteNotifications)]) {
        [[UIApplication sharedApplication] registerForRemoteNotifications];
    }
}
#endif
- (void)application:(UIApplication *)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken
{
    NSString *realDeviceToken = [NSString stringWithFormat:@"%@",deviceToken];
    realDeviceToken = [realDeviceToken stringByReplacingOccurrencesOfString:@"<" withString:@""];
    realDeviceToken = [realDeviceToken stringByReplacingOccurrencesOfString:@">" withString:@""];
    realDeviceToken = [realDeviceToken stringByReplacingOccurrencesOfString:@" " withString:@""];
    NSUserDefaults *user = [NSUserDefaults standardUserDefaults];
    [user setObject:realDeviceToken forKey:@"realDeviceToken"];
    [user synchronize];
    NSLog(@"=======>>>realDeviceToken:%@", realDeviceToken);
}

- (void)application:(UIApplication *)application didReceiveRemoteNotification:(NSDictionary *)userInfo fetchCompletionHandler:(void (^)(UIBackgroundFetchResult))completionHandler
{
    
    if(application.applicationState == UIApplicationStateInactive) {
        
        NSLog(@"Inactive - the user has tapped in the notification when app was closed or in background");
        //do some tasks
        //[self manageRemoteNotification:userInfo];
        NSLog(@"UIApplicationStateInactive userInfo:%@", userInfo);
        NSLog(@"userInfo == %@",userInfo);
        NSString *message = [userInfo objectForKey:@"push"];
        NSLog(@"---notification message:%@", message);
        UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"notification" message:message delegate:self cancelButtonTitle:@"cancel" otherButtonTitles:@"other", nil];
        
        [alert show];
        completionHandler(UIBackgroundFetchResultNewData);
    }
    else if (application.applicationState == UIApplicationStateBackground) {
        
        NSLog(@"application Background - notification has arrived when app was in background");
        NSString* contentAvailable = [NSString stringWithFormat:@"%@", [[userInfo valueForKey:@"aps"] valueForKey:@"content-available"]];
        
        NSLog(@"UIApplicationStateBackground userInfo:%@", userInfo);
        if([contentAvailable isEqualToString:@"1"]) {
            // do tasks
            //[self manageRemoteNotification:userInfo];
            NSLog(@"content-available is equal to 1");
            completionHandler(UIBackgroundFetchResultNewData);
        }
    }
    else {
        NSLog(@"application Active - notication has arrived while app was opened");
        //Show an in-app banner
        //do tasks
        //[self manageRemoteNotification:userInfo];
        NSLog(@"ELSE ELSE ELSE userInfo:%@", userInfo);
        completionHandler(UIBackgroundFetchResultNewData);
    }
}

- (void)application:(UIApplication *)application didReceiveRemoteNotification:(NSDictionary *)userInfo{
    
    //NSLog(@"userInfo == %@",userInfo);
    //NSString *message = [userInfo objectForKey:@"push"];
    //NSLog(@"---notification message:%@", message);
    //
    ////UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"提示" message:message delegate:self cancelButtonTitle:@"取消" otherButtonTitles:@"确定", nil nil];
    //UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"notification" message:message delegate:self cancelButtonTitle:@"cancel" otherButtonTitles:@"other", nil];
    //
    //[alert show];
}

- (void)applicationWillResignActive:(UIApplication *)application {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}

@end
