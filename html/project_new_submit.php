<?php

ob_implicit_flush(1);

// generate a new key
$_REQUEST["key"] = sha1(rand().rand().rand().rand().rand().rand());
include("common.php");

function myflush()
{
    @ob_flush();
    flush();
}

do_header("Creating new project");
do_banner();
myflush();

$PROJECT_DIR = $PROJECTS_PATH."/".$key;
mkdir($PROJECT_DIR);
$PROJECT_PDF = $PROJECT_DIR."/"."upload.pdf";

if (!move_uploaded_file($_FILES['pdffile']['tmp_name'], $PROJECT_PDF)) {
   print "File upload failed.\n";
   die();
}

?>
<!--
<h2>Your key</h2>

<P>Your project has been assigned a unique 'key' which will give anyone who knows it access to your project. <B>If you lose this key, you will lose access to your project.</B></P>

<P><b>Your key: </b><?php print $key ?>

<P>You can also bookmark the next page, whose URL contains your key. </P>
-->

<?php

 if (!str_ends_with(strtolower($_FILES['pdffile']['name']), ".pdf")) {
     print "<h2>Humph.</h2><P>I'm sure your file is neat, but I only know how to handle PDFs. Try exporting your file first.";
     die();
 }

print "<h2>Project creation status</h2>";

print "File uploaded succeeded....<br>\n";
myflush();

print "Splitting PDF into slides... (this can take minutes)<br>\n";
myflush();

system("cd $PROJECT_DIR && pdftoppm -png -scale-to-x -1 -scale-to-y 1080 $PROJECT_PDF pdfslide");

print "Almost there...<br>\n";
myflush();

system("cd $PROJECT_DIR && pdftoppm -png -scale-to-x -1 -scale-to-y 320 $PROJECT_PDF pdfthumb");

$files = array();
$dir = opendir($PROJECT_DIR);
while (($file = readdir($dir)) !== false) {
  if (str_starts_with($file, "pdfslide"))
     $files[] = $file;
}

sort($files);

if (count($files) == 0) {
    print "<h2>I couldn't decode any pages. Is this a PDF?</h2>";
    die();
}

closedir($dir);
?>

<script>

var slide_names = [];
<?php
  for ($i = 0; $i < count($files); $i++) {
     print "slide_names.push(\"".$files[$i]."\");\n";
  }
?>

// create the initial document.
var doc = new Object();
doc.key = "<?php print $key ?>";
doc.slides = [];
var nslides = slide_names.length;

var max_seconds = <?php print $MAX_MOVIE_SECONDS ?>;

for (i = 0; i < nslides; i++) {
    var slide = new Object();
    slide['type'] = "slide";
    slide['slide'] = slide_names[i];
    slide['thumb'] = slide_names[i].replace("pdfslide", "pdfthumb");
    slide['progress'] = 1;

    slide['seconds'] = Math.floor(max_seconds * 10 / nslides) / 10;

    doc.slides.push(slide);
}

doc.progress = 1;
doc.display_idx = 0;
doc.name = "<?php print $_FILES['pdffile']['name'] ?>";
doc.create_date = new Date();
doc.create_ip = "<?php print $_SERVER["REMOTE_ADDR"] ?>";
doc_save();

doc_register_in_local_storage();

</script>

<?php

print "<h2>Your project has been created!</h2>";

print "<a href=project.php?key=$key>Go to project page</a><br>";
print "&nbsp;&nbsp;<img src=bookmarkme.png><br>";

do_footer();
?>
