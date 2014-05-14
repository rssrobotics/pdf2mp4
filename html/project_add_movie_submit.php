<?php

include("common.php");

do_header("Project editor");
do_banner();

validate_key($key);

$MOVIE_NAME = "video_".sha1(rand().rand().rand().rand());

$MOVIE_DIR = $PROJECT_DIR."/".$MOVIE_NAME;
mkdir($MOVIE_DIR);

print "<h2>Movie import status</h2>";

print "File uploaded succeeded....<br><br>\n";

print "Extracting frames...<br><br>\n";

myflush();

system("avconv -i ".$_FILES['moviefile']['tmp_name']." -f image2 $MOVIE_DIR/frame%08d.png");

//if (!move_uploaded_file($_FILES['moviefile']['tmp_name'], $PROJECT_PDF)) {
//   print "File upload failed.\n";
//   die();
//}

$frames = array();
$dir = opendir($MOVIE_DIR);
while (($file = readdir($dir)) !== false) {
  if (str_starts_with($file, "frame"))
     $frames[] = $file;
}

sort($frames);

if (count($frames) == 0) {
    print "<h2>I couldn't decode any frames. Is this an MP4?</h2>";
    die();
}

closedir($dir);
print "<h2>Got ".count($frames)." frames.</h2>";

?>

<script>
xmlhttp_post("project_read.php", "key=<?php print $key ?>",
             function(xml, url) {
                 try {
                     doc = JSON.parse(xml.responseText);

                     var newslide = new Object();
                     newslide['type'] = "movie";
                     newslide['dir'] = "<?php print $MOVIE_NAME ?>";
                     newslide['thumb'] = "<?php print $MOVIE_NAME."/".$frames[count($frames)/2] ?>";
                     newslide['progress'] = 1;
                     newslide['playfps'] = <?php print $_REQUEST["playfps"] ?>;
                     newslide['seconds'] = <?php print floatval($_REQUEST["seconds"]) ?>;
                     newslide['mode'] = "<?php print $_REQUEST["mode"] ?>";
                     doc.slides.splice(<?php print $_REQUEST["idx"] ?>, 0, newslide);

                     doc_save_real();

                 } catch (ex) {
                     alert("error: "+xml.statusText);
                 }
             });
</script>

<center>
<h2><a href=project.php?key=<?php print $key?>>Great, take me back to the project</a></h2>
</center>

<?php
do_footer();
?>
