
function toNavigator()
{
	CycleWindow('navigator:browser', 'chrome://navigator/content/');
}

function toMessengerWindow()
{
	// FIX ME - Really need to find the front most messenger window
	//          and bring it all the way to the front
	
	window.open("chrome://messenger/content/", "messenger", "chrome");
}


function toAddressBook() 
{
	var wind = window.open("chrome://addressbook/content/addressbook.xul",
						   "addressbook", "chrome");
	return wind;
}

function toHistory()
{
	var toolkitCore = XPAppCoresManager.Find("toolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("toolkitCore");
      }
    }
    if (toolkitCore) {
      toolkitCore.ShowWindow("resource://res/samples/history.xul",window);
    }
}

function CycleWindow( inType, inChromeURL )
{
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	dump("got window Manager \n");
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    dump("got interface \n");
    
    var desiredWindow = null;
    
	topWindowOfType = windowManagerInterface.GetMostRecentWindow( inType );
	topWindow = windowManagerInterface.GetMostRecentWindow( null );
	dump( "got windows \n");
	if ( topWindowOfType != topWindow )
	{
		dump( "first not top so give focus \n");
		topWindowOfType.focus();
		return;
	}
	
	if ( topWindowOfType == null )
	{
		dump( " no windows of this type so create a new one \n");
		window.open( inChromeURL, "","chrome" );
	}
	var enumerator = windowManagerInterface.GetEnumerator( inType );
	firstWindow = windowManagerInterface.ConvertISupportsToDOMWindow ( enumerator.GetNext() );
	if ( firstWindow == topWindowOfType )
	{
		dump( "top most window is first window \n");
		firstWindow = null;
	}
	else
	{
		dump("find topmost window \n");
		while ( enumerator.HasMoreElements() )
		{
			var nextWindow = windowManagerInterface.ConvertISupportsToDOMWindow ( enumerator.GetNext() );
			if ( nextWindow == topWindowOfType )
				break;
		}	
	}
	desiredWindow = firstWindow;
	if ( enumerator.HasMoreElements() )
	{
		dump( "Give focus to next window in the list \n");
		desiredWindow = windowManagerInterface.ConvertISupportsToDOMWindow ( enumerator.GetNext() );		
	}
	
	if ( desiredWindow )
	{
		desiredWindow.focus();
		dump("focusing window \n");
	}
	else
	{
		dump("open window \n");
		window.open( inChromeURL, "","chrome" );
	}
}

function toEditor()
{
	var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
	if (!toolkitCore) {
	  toolkitCore = new ToolkitCore();
	  if (toolkitCore) {
	    toolkitCore.Init("ToolkitCore");
	  }
	}
	if (toolkitCore) {
	  toolkitCore.ShowWindowWithArgs("chrome://editor/content/EditorAppShell.xul",window,"chrome://editor/content/EditorInitPage.html");
	}
}

function toNewTextEditorWindow()
{
	core = XPAppCoresManager.Find("toolkitCore");
	if ( !core ) {
	    core = new ToolkitCore();
	    if ( core ) {
	        core.Init("toolkitCore");
	    }
	}
	if ( core ) {
	    core.ShowWindowWithArgs( "chrome://editor/content/TextEditorAppShell.xul", window, "chrome://editor/content/EditorInitPagePlain.html" );
	} else {
	    dump("Error; can't create toolkitCore\n");
	}
}
function ShowWindowFromResource( node )
{
	var windowManager = Components.classes['component://netscape/rdf/datasource?name=window-mediator'].getService();
	dump("got window Manager \n");
	var	windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    dump("got interface \n");
    
    var desiredWindow = null;
    var url = node.getAttribute('id');
    dump( url +" finding \n" );
	desiredWindow = windowManagerInterface.GetWindowForResource( url );
	dump( "got window \n");
	if ( desiredWindow )
	{
		dump("focusing \n");
		desiredWindow.focus();
	}
}

