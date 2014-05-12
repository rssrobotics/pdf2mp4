<?php

$PROJECTS_PATH="/var/www/pdf2mp4/html/projects";
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

$key = $_REQUEST["key"];

$PROJECT_DIR = $PROJECTS_PATH."/".$key;
$PROJECT_JSON = $PROJECT_DIR."/"."json.txt";
$PROJECT_URL = $PROJECTS_URL."/".$key;

$MAX_MOVIE_SECONDS = 300;

function validate_key($key)
{
    global $PROJECT_JSON;

    if (strlen($key) != 40 || !preg_match("/^[a-z0-9]*$/", $key) || !file_exists($PROJECT_JSON)) {
        print "<h2>Sorry, invalid key.</h2>";
        do_footer();
        die();
    }

    return true;
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

function str_ends_with($haystack, $needle)
{
    return $needle === "" || substr($haystack, -strlen($needle)) === $needle;
}

function str_starts_with($haystack, $needle)
{
    return $needle === "" || strpos($haystack, $needle) === 0;
}

?>
