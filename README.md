Big Screens Quicktime Player
============================

Cinder support for playing large Quicktime movies across multiple screens.

One Player acts as the "clock" over a UDP socket to synchronize all of the Players.

###Usage
* Distribute a copy of the Player to each machine.  
  For development, you can also run multiple Players on a single machine.
  
* Modify the settings.xml file for each Player. See below for details.  

* Launch the Players. 

####Settings
Each Player must have a "settings.xml" file in the same directory as the Player. 
If you're running multiple Players on a single machine, just
put them each in their own subfolder (like they're structured in the repo). 

```xml
<settings>
  <!-- Determines which machine is the clock. There can be only 1. -->  
  <is_clock>false</is_clock>
  <!-- The absolute path to the Quicktime compatible video file. -->  
  <movie_path>/Users/bill/Desktop/whistle.mov</movie_path>
  <!-- 
    The port that the non-clock clients are listening for timing messages on. 
    Not required for the clock Player.
  -->
  <listen_port>9004</listen_port>
  <!-- 
    The port(s) that the clock is broadcasting to. 
    This can be a single port if the Players are on different machines,
    but there must be a unique port for each Player on a single machine.
    The later case is useful for development.
    Not required for non-clock Players.
  -->
  <send_port>9004</send_port>
  <send_port>9005</send_port>
  <!-- The rest of these settings are borrowed from Most-Pixels-Ever -->
  <!-- The size of the Player crop -->
	<local_dimensions>
		<width>400</width>
		<height>400</height>
	</local_dimensions>
  <!-- The position of the Player crop -->
	<local_location>
		<x>0</x>
		<y>0</y>
	</local_location>
  <!-- 
    The size of the combined screens. 
    It is assumed that the video is the same size. 
  -->
	<master_dimensions>
		<width>1200</width>
		<height>400</height>
	</master_dimensions>
  <!-- 
    Forces the app into full-screen mode upon launch. 
    The local_dimensions should be the same as the screen dimensions. 
  -->  
  <go_fullscreen>false</go_fullscreen>
  <!-- Positions the Player window within the screen. -->
  <offset_window>true</offset_window>
</settings>
```