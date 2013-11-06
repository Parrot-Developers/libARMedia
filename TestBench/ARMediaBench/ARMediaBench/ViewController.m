//
//  ViewController.m
//  ARMediaBench
//
//  Created by Malick Sylla  on 10/10/13.
//  Copyright (c) 2013 Parrot SA. All rights reserved.
//

#import "ViewController.h"

@interface ViewController ()

@end

@implementation ViewController
@synthesize addMedia = _addMedia;
@synthesize addNilMedia = _addNilMedia;
@synthesize addVideoMedia = _addVideoMedia;
@synthesize pictureFolderPath = _pictureFolderPath;
@synthesize videoFolderPath = _videoFolderPath;
@synthesize delosVideoFolderPath = _delosVideoFolderPath;
@synthesize retreiveProjectDic = _retreiveProjectDic;

- (void)callback:(NSNotification *)notification
{
    NSLog(@"%@:%@", notification.name, notification.object);
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(callback:) name:kARMediaManagerNotificationUpdated object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(callback:) name:kARMediaManagerNotificationUpdating object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(callback:) name:kARMediaManagerNotificationInitialized object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(callback:) name:kARMediaManagerNotificationMediaAdded object:nil];

    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *error;
    NSString *documentsDirectory = [paths objectAtIndex:0];
    
    NSString *txtPath = [documentsDirectory stringByAppendingPathComponent:@"IMG_1115.mov"];
    NSString *resourcePath = [[NSBundle mainBundle] pathForResource:@"IMG_1115" ofType:@"mov"];
    [fileManager copyItemAtPath:resourcePath toPath:txtPath error:&error];
    self.videoFolderPath = txtPath;

    NSString *txtPath2 = [documentsDirectory stringByAppendingPathComponent:@"IMG_delos_pvat2.mov"];
    NSString *resourcePath2 = [[NSBundle mainBundle] pathForResource:@"IMG_delos_pvat2" ofType:@"mov"];
    [fileManager copyItemAtPath:resourcePath2 toPath:txtPath2 error:&error];
    self.delosVideoFolderPath = txtPath2;
    
    NSString *picturePath = [documentsDirectory stringByAppendingPathComponent:@"IMG_1155.JPG"];
    NSString *pictureResourcePath = [[NSBundle mainBundle] pathForResource:@"IMG_1155" ofType:@"JPG"];
    [fileManager copyItemAtPath:pictureResourcePath toPath:picturePath error:&error];
    self.pictureFolderPath = picturePath;
    
    self.retreiveProjectDic = [NSMutableDictionary dictionary];
}


- (IBAction)addMedia:(UIButton *)sender
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [[ARMedia_Manager sharedARMedia_Manager]ARMedia_Manager_AddMedia:self.pictureFolderPath];
    });
}

- (IBAction)addNilMedia:(UIButton *)sender
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [[ARMedia_Manager sharedARMedia_Manager ]ARMedia_Manager_AddMedia:nil];
    });
}

- (IBAction)addVideoMedia:(UIButton *)sender
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [[ARMedia_Manager sharedARMedia_Manager ]ARMedia_Manager_AddMedia:self.videoFolderPath];
    });
}

- (IBAction)addVideoDelosMedia:(UIButton *)sender
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [[ARMedia_Manager sharedARMedia_Manager ]ARMedia_Manager_AddMedia:self.delosVideoFolderPath];
    });
}

- (IBAction)ARMediaLibrary:(UIButton *)sender
{
    [[ARMedia_Manager sharedARMedia_Manager] ARMedia_Manager_InitWithProjectIDs:[NSArray arrayWithObjects:@"Parrot AR.Drone", @"Parrot Delos", nil]];
}

- (IBAction)updateARMediaLibrary:(UIButton *)sender
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        [[ARMedia_Manager sharedARMedia_Manager] ARMedia_Manager_Update];
    });
}

- (IBAction)projectDictionary:(UIButton *)sender
{
   /* dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
        NSLog(@"DONE = %@",[[ARMedia_Manager sharedARMedia_Manager] getRefreshedProjectMediaDictionary:@"Parrot AR.Drone"]);
    });*/
    [self.retreiveProjectDic removeObjectForKey:@"Parrot AR.Drone"];
    NSLog(@"retreiveProjectDic : %@",self.retreiveProjectDic);
}

- (IBAction)allProjectDictionary:(UIButton *)sender
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        
       [self.retreiveProjectDic setDictionary:[[ARMedia_Manager sharedARMedia_Manager] ARMedia_Manager_GetProjectDictionary:nil]];
        NSLog(@"retreiveProjectDic : %@",self.retreiveProjectDic);
    });
}


- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end