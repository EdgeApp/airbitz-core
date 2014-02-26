//
//  TextFieldContainerView.h
//  Beacon
//
//  Created by Adam Harris on 7/4/13.
//  Copyright (c) 2013 Ditty Labs. All rights reserved.
//

#import <UIKit/UIKit.h>

@protocol TextFieldContainerViewDelegate;

@interface TextFieldContainerView : UIView <UITextFieldDelegate>

@property (nonatomic, assign) id<TextFieldContainerViewDelegate>    delegateContainer;
@property (nonatomic, retain) NSArray                               *arrayTextFields;

- (void)initialize;
- (void)resignAllResponders;
- (void)moveToNextResponderAfter:(UITextField *)textField;

@end

@protocol TextFieldContainerViewDelegate <NSObject>

@optional
- (void)textFieldDidBeginEditing:(UITextField *)controller;
- (void)textFieldDidEndEditing:(UITextField *)controller;
- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string;
- (BOOL)textFieldShouldBeginEditing:(UITextField *)textField;
- (void)textFieldGoSelected:(UITextField *)textField;
- (void)textFieldNextSelected:(UITextField *)textField;


@end