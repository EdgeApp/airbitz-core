//
//  TextFieldContainerView.m
//  Beacon
//
//  Created by Adam Harris on 7/4/13.
//  Copyright (c) 2013 Ditty Labs. All rights reserved.
//

#import "TextFieldContainerView.h"

#define DISTANCE_ABOVE_KEYBOARD             10  // how far above the keyboard to we want the control
#define ANIMATION_DURATION_KEYBOARD_UP      0.30
#define ANIMATION_DURATION_KEYBOARD_DOWN    0.25


@interface TextFieldContainerView ()
{
    BOOL    _bInitialized;
    BOOL    _bKeyboardIsShown;
    CGRect  _frameStart;
    CGFloat _keyboardHeight;
}

@property (nonatomic, retain) UIButton  *buttonBackground;

- (UITextField *)firstResponder;
- (CGFloat)obscuredAmountFor:(UIView *)theView;
- (void)moveToClearKeyboardFor:(UIView *)theView withDuration:(CGFloat)duration;

@end

@implementation TextFieldContainerView

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        // init code here
    }
    return self;
}

- (void)awakeFromNib
{
    [self initialize];
}


- (void)dealloc
{
    //remove all notifications associated with self
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect
{
    // Drawing code
}
*/

#pragma mark - Public Methods

- (void)initialize
{
    if (!_bInitialized)
    {
        _frameStart = self.frame;
        _keyboardHeight = 0.0;
        
        // register for keyboard notifications
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(keyboardWillShow:)
                                                     name:UIKeyboardWillShowNotification
                                                   object:self.window];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(keyboardWillHide:)
                                                     name:UIKeyboardWillHideNotification
                                                   object:self.window];
        
        _bKeyboardIsShown = NO;
        
        // create the background button that we'll use to dismiss the keyboard if they touch the 'background'
        self.buttonBackground = [UIButton buttonWithType:UIButtonTypeCustom];
        [self.buttonBackground addTarget:self action:@selector(backgroundTouched:) forControlEvents:UIControlEventTouchUpInside];
        self.buttonBackground.hidden = YES;
        self.buttonBackground.backgroundColor = [UIColor clearColor];
        CGRect frame;
        frame.origin.x = 0;
        frame.origin.y = 0;
        frame.size = self.frame.size;
        self.buttonBackground.frame = frame;
        [self addSubview:self.buttonBackground];
        [self bringSubviewToFront:self.buttonBackground];

        // run through all the text field views attached to us
        NSMutableArray *arrayTextFields = [[NSMutableArray alloc] init];
        for (UIView *view in self.subviews)
        {
            if ([view isKindOfClass:[UITextField class]])
            {
                UITextField *textField = (UITextField *) view;
                
                // add to our array
                [arrayTextFields addObject:textField];
                
                // make sure they are in front of our background button
                [self bringSubviewToFront:textField];
                
                // set ourselves as delegate
                textField.delegate = self;
            }
        }
        
        // sort the array from top of view to bottom
        NSArray *arraySorted;
        arraySorted = [arrayTextFields sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
            UITextField *first = (UITextField *)a;
            UITextField *second = (UITextField *)b;
            NSComparisonResult result = NSOrderedSame;
            if (first.frame.origin.y < second.frame.origin.y)
            {
                result = NSOrderedAscending;
            }
            else if (first.frame.origin.y < second.frame.origin.y)
            {
                result = NSOrderedDescending;
            }
            
            return result;
        }];
        
        self.arrayTextFields = arraySorted;
        
        //NSLog(@"array count = %d", [self.arrayTextFields count]);
        
        _bInitialized = YES;
    }
}

// resigns all the edit box responders
- (void)resignAllResponders
{
    for (UITextField *curTextField in self.arrayTextFields)
    {
        [curTextField resignFirstResponder];
    }
}

- (void)moveToNextResponderAfter:(UITextField *)textField
{
    NSInteger indexOfField = [self.arrayTextFields indexOfObject:textField];
    
    if (indexOfField != NSNotFound)
    {
        // if this is the last field or the return key is not next
        if ((indexOfField == [self.arrayTextFields count] - 1) || (UIReturnKeyNext != textField.returnKeyType))
        {
            // dismiss the keyboard
            [textField resignFirstResponder];
        }
        else
        {
            // get the next field
            UITextField *nextField = [self.arrayTextFields objectAtIndex:indexOfField + 1];
            [nextField becomeFirstResponder];
        }
    }
    else
    {
        [textField resignFirstResponder];
    }
}

#pragma mark - Misc Methods

// get's the first responder
- (UITextField *)firstResponder
{
    UITextField *responder = nil;
    
    for (UITextField *curTextField in self.arrayTextFields)
    {
        if ([curTextField isFirstResponder])
        {
            responder = curTextField;
            break;
        }
    }
    
    return responder;
}

// returns how much the current first responder is obscured by the keyboard
// negative means above the keyboad by that amount
- (CGFloat)obscuredAmountFor:(UIView *)theView
{
    CGFloat obscureAmount = 0.0;
    
    // determine how much we are obscured if any
    if (theView)
    {
        UIWindow *frontWindow = [[UIApplication sharedApplication] keyWindow];
        
        CGPoint pointInWindow = [frontWindow.rootViewController.view convertPoint:theView.frame.origin fromView:self];
        
        CGFloat distFromBottom = frontWindow.frame.size.height - pointInWindow.y;
        obscureAmount = (_keyboardHeight + theView.frame.size.height) - distFromBottom;
        
        //NSLog(@"y coord = %f", theView.frame.origin.y);
        //NSLog(@"y coord in window = %f", pointInWindow.y);
        //NSLog(@"dist from bottom = %f", distFromBottom);
        //NSLog(@"amount Obscured = %f", obscureAmount);
    }
    
    return obscureAmount;
}

- (void)moveToClearKeyboardFor:(UIView *)theView withDuration:(CGFloat)duration
{
    CGRect newFrame = self.frame;
    
    // determine how much we are obscured
    CGFloat obscureAmount = [self obscuredAmountFor:theView];
    obscureAmount += (CGFloat) DISTANCE_ABOVE_KEYBOARD;
    
    // if obscured too much
    //NSLog(@"obscure amount final = %f", obscureAmount);
    if (obscureAmount != 0.0)
    {
        // it is obscured so move it to compensate
        //NSLog(@"need to compensate");
        newFrame.origin.y -= obscureAmount;
    }
    
    //NSLog(@"old origin: %f, new origin: %f", _frameStart.origin.y, newFrame.origin.y);
    
    // if our new position puts us lower then we were originally
    if (newFrame.origin.y > _frameStart.origin.y)
    {
        newFrame.origin.y = _frameStart.origin.y;
    }
    
    // if we need to move
    if (self.frame.origin.y != newFrame.origin.y)
    {
        
        [UIView animateWithDuration:duration
                              delay:0.0
                            options:UIViewAnimationOptionCurveEaseInOut
                         animations:^{
                             self.frame = newFrame;
                         }
                         completion:^(BOOL finished){
                             
                             
                         }
         ];
    }
}

- (BOOL)isOneOfInputViews:(UIView *)viewToCheck
{
    BOOL bRetVal = NO;
    
    if ([self.arrayTextFields containsObject:viewToCheck])
    {
        bRetVal = YES;
    }
    
    return bRetVal;
}

#pragma mark - Action Methods

// called when one of the background buttons is touched
- (IBAction)backgroundTouched:(id)sender
{
    // hide the keyboard
    [self resignAllResponders];
}

#pragma mark - UITextField Delegate

// called when user hits 'done' in keyboard
- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    // if this field has a next key
    if (UIReturnKeyNext == textField.returnKeyType)
    {
        // if they have set the delegate function
        if ([self.delegateContainer respondsToSelector:@selector(textFieldNextSelected:)])
        {
            [self.delegateContainer textFieldNextSelected:textField];
        }
        else
        {
            [self moveToNextResponderAfter:textField];
        }
    }
    else
    {
        // dismss the keyboard
        [textField resignFirstResponder];
        
        // if it has a 'go' key
        if (UIReturnKeyGo == textField.returnKeyType)
        {
            // if they have set the delegate function
            if ([self.delegateContainer respondsToSelector:@selector(textFieldGoSelected:)])
            {
                [self.delegateContainer textFieldGoSelected:textField];
            }
        }
    }
    
    return NO;
}

- (void)textFieldDidBeginEditing:(UITextField *)textField
{
    //NSLog(@"textFieldDidBeginEditing");
    
    // if they have set the delegate function
    if ([self.delegateContainer respondsToSelector:@selector(textFieldDidBeginEditing:)])
    {
        [self.delegateContainer textFieldDidBeginEditing:textField];
    }
    
    // if the keyboard is already showing
    if (_bKeyboardIsShown)
    {
        // move ourselves up to clear the keyboard
        [self moveToClearKeyboardFor:textField withDuration:ANIMATION_DURATION_KEYBOARD_UP];
    }
}

- (void)textFieldDidEndEditing:(UITextField *)textField
{
    // if they have set the delegate function
    if ([self.delegateContainer respondsToSelector:@selector(textFieldDidEndEditing:)])
    {
        [self.delegateContainer textFieldDidEndEditing:textField];
    }
}

- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
    BOOL bShouldChange = YES;
    
    // if they have set the delegate function
    if ([self.delegateContainer respondsToSelector:@selector(textField: shouldChangeCharactersInRange: replacementString:)])
    {
        bShouldChange = [self.delegateContainer textField:textField shouldChangeCharactersInRange:range replacementString:string];
    }
    
    return bShouldChange;
}

- (BOOL)textFieldShouldBeginEditing:(UITextField *)textField
{
    BOOL bShouldBeginEditing = YES;
    
    // if they have set the delegate function
    if ([self.delegateContainer respondsToSelector:@selector(textFieldShouldBeginEditing:)])
    {
        bShouldBeginEditing = [self.delegateContainer textFieldShouldBeginEditing:textField];
    }
    
    return bShouldBeginEditing;
}

#pragma mark - Keyboard Notification Methods

- (void)keyboardWillShow:(NSNotification *)n
{
    //NSLog(@"keyboardWillShow");
    
    // by-pass if we are hidden
    if (self.hidden)
    {
        return;
    }

    // NOTE: The keyboard notification will fire even when the keyboard is already shown.
    if (_bKeyboardIsShown)
    {
        return;
    }
    
    // get the first responder
    UITextField *textFieldFirstResponder = [self firstResponder];
    
    if (![self isOneOfInputViews:textFieldFirstResponder])
    {
        return;
    }
    
    // get the height of the keyboard
    NSDictionary* userInfo = [n userInfo];
    NSValue* boundsValue = [userInfo objectForKey:UIKeyboardFrameEndUserInfoKey];
    CGSize keyboardSize = [boundsValue CGRectValue].size;
    _keyboardHeight = keyboardSize.height;
    
    // move ourselves up to clear the keyboard
    [self moveToClearKeyboardFor:textFieldFirstResponder withDuration:ANIMATION_DURATION_KEYBOARD_UP];
    
    _bKeyboardIsShown = YES;
    
    _buttonBackground.hidden = NO;
}

- (void)keyboardWillHide:(NSNotification *)n
{
    if (self.hidden)
    {
        return;
    }
    
    if (self.frame.origin.y != _frameStart.origin.y)
    {
        [UIView animateWithDuration:ANIMATION_DURATION_KEYBOARD_DOWN
                              delay:0.0
                            options:UIViewAnimationOptionCurveEaseInOut
                         animations:^{
                             self.frame = _frameStart;
                         }
                         completion:^(BOOL finished){
                         }
         ];
    }
    
    _buttonBackground.hidden = YES;
    
    _bKeyboardIsShown = NO;
    
    _keyboardHeight = 0.0;
}

@end
