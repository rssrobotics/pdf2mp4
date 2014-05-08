<?php

$PROJECTS_PATH="/var/www/html/projects";
$PROJECTS_URL="http://localhost/projects";

function do_header($name)
{
print "<html><head><title>$name</title>";
?>
<link href=styles.css rel=stylesheet type="text/css">
<link rel="icon" href="favicon.ico" />
</head>

<script type="text/javascript" src="pdf2mp4.js"></script>

<body>

<?php
}

function do_banner()
{
?>
<center>
<a href="/"><img border=0 src=pdf2mp4.png></a>
<P>Create a video from a pages in a PDF</P>
</center>

<?php
}

function do_footer()
{
	print "<br clear=all><br><hr>";
	print "<center>";
	print "<font face=-1>Developed by <a href=mailto:ebolson@umich.edu>Edwin Olson</a> for the Robotics Science and Systems 2014 conference.</font>";
	print "</center>";
	print "</html";
}

?>
