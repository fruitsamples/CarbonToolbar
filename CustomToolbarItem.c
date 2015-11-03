/*
    File:		CustomToolbarItem.c
    
    Version:	Mac OS X

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
				("Apple") in consideration of your agreement to the following terms, and your
				use, installation, modification or redistribution of this Apple software
				constitutes acceptance of these terms.  If you do not agree with these terms,
				please do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following terms, and subject
				to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs
				copyrights in this original Apple software (the "Apple Software"), to use,
				reproduce, modify and redistribute the Apple Software, with or without
				modifications, in source and/or binary forms; provided that if you redistribute
				the Apple Software in its entirety and without modifications, you must retain
				this notice and the following text and disclaimers in all such redistributions of
				the Apple Software.  Neither the name, trademarks, service marks or logos of
				Apple Computer, Inc. may be used to endorse or promote products derived from the
				Apple Software without specific prior written permission from Apple.  Except as
				expressly stated in this notice, no other rights or licenses, express or implied,
				are granted by Apple herein, including but not limited to any patent rights that
				may be infringed by your derivative works or by other works in which the Apple
				Software may be incorporated.

				The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
				WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
				WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
				PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
				COMBINATION WITH YOUR PRODUCTS.

				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
				CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
				GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
				ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
				OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
				(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
				ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	Copyright © 2002 Apple Computer, Inc., All Rights Reserved
*/

#include "CustomToolbarItem.h"

const EventTypeSpec kEvents[] = 
{
	{ kEventClassHIObject, kEventHIObjectConstruct },
	{ kEventClassHIObject, kEventHIObjectInitialize },
	{ kEventClassHIObject, kEventHIObjectDestruct },
	
	{ kEventClassToolbarItem, kEventToolbarItemGetPersistentData },
	{ kEventClassToolbarItem, kEventToolbarItemPerformAction }
};

#define kCustomToolbarItemClassID		CFSTR( "com.voas.customtoolbaritem" )

struct CustomToolbarItem
{
	HIToolbarItemRef		toolbarItem;
	CFURLRef				url;
};
typedef struct CustomToolbarItem CustomToolbarItem;

static OSStatus			ConstructCustomToolbarItem( HIToolbarItemRef inItem, CustomToolbarItem** outItem );
static void				DestructCustomToolbarItem( CustomToolbarItem* inItem );
static OSStatus			InitializeCustomToolbarItem( CustomToolbarItem* inItem,  EventRef inEvent );
static CFTypeRef		CreateCustomToolbarItemPersistentData( CustomToolbarItem* inItem );
static pascal OSStatus	CustomToolbarItemHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData );

//-----------------------------------------------------------------------------
//	RegisterCustomToolbarItemClass
//-----------------------------------------------------------------------------
//	Our class registration call. We take care to only register once. We call this
//	in our CreateCustomToolbarItem call below to make sure our class is in place
//	before trying to instantiate an object of the class.
//
void
RegisterCustomToolbarItemClass()
{
	static bool sRegistered;
	
	if ( !sRegistered )
	{
		HIObjectRegisterSubclass( kCustomToolbarItemClassID, kHIToolbarItemClassID, 0,
				CustomToolbarItemHandler, GetEventTypeCount( kEvents ), kEvents, 0, NULL );
		
		sRegistered = true;
	}
}

//-----------------------------------------------------------------------------
//	CreateCustomToolbarItem
//-----------------------------------------------------------------------------
//	Our 'public' API to create our custom URL item.
//
HIToolbarItemRef
CreateCustomToolbarItem( CFStringRef inIdentifier, CFTypeRef inURL )
{
	OSStatus			err;
	EventRef			event;
	UInt32				options = kHIToolbarItemAllowDuplicates;
	HIToolbarItemRef	result = NULL;
	
	RegisterCustomToolbarItemClass();
	
	err = CreateEvent( NULL, kEventClassHIObject, kEventHIObjectInitialize,
			GetCurrentEventTime(), 0, &event );
	require_noerr( err, CantCreateEvent );

	SetEventParameter( event, kEventParamToolbarItemIdentifier, typeCFStringRef, sizeof( CFStringRef ), &inIdentifier );
	SetEventParameter( event, kEventParamAttributes, typeUInt32, sizeof( UInt32 ), &options );

	if ( inURL )
		SetEventParameter( event, kEventParamToolbarItemConfigData, typeCFTypeRef, sizeof( CFTypeRef ), &inURL );
	
	err = HIObjectCreate( kCustomToolbarItemClassID, event, (HIObjectRef*)&result );
	check_noerr( err );

	ReleaseEvent( event );
	
CantCreateEvent:
	return result;
}

//-----------------------------------------------------------------------------
//	ConstructCustomToolbarItem
//-----------------------------------------------------------------------------
//	Create a new custom toolbar item with no URL yet.
//
static OSStatus
ConstructCustomToolbarItem( HIToolbarItemRef inItem, CustomToolbarItem** outItem )
{
	CustomToolbarItem*		item;
	OSStatus				err = noErr;
	
	item = (CustomToolbarItem*)malloc( sizeof( CustomToolbarItem ) );
	require_action( item != NULL, CantAllocItem, err = memFullErr );
	
	item->toolbarItem = inItem;
	item->url = NULL;
	
	*outItem = item;

CantAllocItem:
	return err;
}

//-----------------------------------------------------------------------------
//	DestructCustomToolbarItem
//-----------------------------------------------------------------------------
//	Destroy our custom item. Be sure to release our URL.
//
static void
DestructCustomToolbarItem( CustomToolbarItem* inItem )
{
	if ( inItem->url )
		CFRelease( inItem->url );

	free( inItem );
}

//-----------------------------------------------------------------------------
//	InitializeCustomToolbarItem
//-----------------------------------------------------------------------------
//	This is called after our item has been constructed. We are called here so
//	that we can pull parameters out of the Carbon Event that is passed into the
//	HIObjectCreate call.
//
static OSStatus
InitializeCustomToolbarItem( CustomToolbarItem* inItem, EventRef inEvent )
{
	CFTypeRef		data;
	IconRef			iconRef;
	
	if ( GetEventParameter( inEvent, kEventParamToolbarItemConfigData, typeCFTypeRef, NULL,
			sizeof( CFTypeRef ), NULL, &data ) == noErr )
	{
		if ( CFGetTypeID( data ) == CFStringGetTypeID() )
			inItem->url = CFURLCreateWithString( NULL, (CFStringRef)data, NULL );
		else
			inItem->url = (CFURLRef)CFRetain( data );
	}
	else
	{
		inItem->url = CFURLCreateWithString( NULL, CFSTR( "http://www.apple.com" ), NULL );
	}

	HIToolbarItemSetLabel( inItem->toolbarItem, CFSTR( "URL Item" ) );
	
	if ( GetIconRef( kOnSystemDisk, kSystemIconsCreator, kGenericURLIcon, &iconRef ) == noErr )
	{
		HIToolbarItemSetIconRef( inItem->toolbarItem, iconRef );
		ReleaseIconRef( iconRef );
	}
	
	HIToolbarItemSetHelpText( inItem->toolbarItem, CFURLGetString( inItem->url ), NULL );
	
	return noErr;
}

//-----------------------------------------------------------------------------
//	CustomToolbarItemHandler
//-----------------------------------------------------------------------------
//	This is where the magic happens. This is your method reception desk. When
//	your object is created, etc. or receives events, this is where we take the
//	events and call specific functions to do the work.
//
static pascal OSStatus
CustomToolbarItemHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData )
{
	OSStatus			result = eventNotHandledErr;
	CustomToolbarItem*	object = (CustomToolbarItem*)inUserData;

	switch ( GetEventClass( inEvent ) )
	{
		case kEventClassHIObject:
			switch ( GetEventKind( inEvent ) )
			{
				case kEventHIObjectConstruct:
					{
						HIObjectRef			toolbarItem;
						CustomToolbarItem*	item;
						
						GetEventParameter( inEvent, kEventParamHIObjectInstance, typeHIObjectRef, NULL,
								sizeof( HIObjectRef ), NULL, &toolbarItem );
						
						result = ConstructCustomToolbarItem( toolbarItem, &item );
						
						if ( result == noErr )
							SetEventParameter( inEvent, kEventParamHIObjectInstance, 'void', sizeof( void * ), &item );
					}
					break;

				case kEventHIObjectInitialize:
					if ( CallNextEventHandler( inCallRef, inEvent ) == noErr )
						result = InitializeCustomToolbarItem( object, inEvent );
					break;
				
				case kEventHIObjectDestruct:
					DestructCustomToolbarItem( object );
					result = noErr;
					break;
			}
			break;
		
		case kEventClassToolbarItem:
			switch ( GetEventKind( inEvent ) )
			{
				case kEventToolbarItemGetPersistentData:
					{
						CFTypeRef		data;
						
						data = CreateCustomToolbarItemPersistentData( object );
						if ( data )
						{
							SetEventParameter( inEvent, kEventParamToolbarItemConfigData, typeCFTypeRef,
								sizeof( CFTypeRef ), &data );
							result = noErr;
						}
					}
					break;
		
				case kEventToolbarItemPerformAction:
					if ( object->url )
						result = LSOpenCFURLRef( object->url, NULL );
					else
						result = noErr;
					break;
			}
			break;
	}
	
	return result;
}


//-----------------------------------------------------------------------------
//	CreateCustomToolbarItemPersistentData
//-----------------------------------------------------------------------------
//	This is called when the toolbar is about to write the config for this item
//	to preferences. It is your chance to save any extra data with the item. You
//	must make sure the data is something that can be saved to XML.
//
static CFTypeRef
CreateCustomToolbarItemPersistentData( CustomToolbarItem* inItem )
{
	CFTypeRef	result = NULL;

	if ( inItem->url )
	{
		return (CFTypeRef)CFStringCreateCopy( NULL, CFURLGetString( inItem->url ) ); 
	}
	
	return result;
}
