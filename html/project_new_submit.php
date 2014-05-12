<?php

ob_implicit_flush(1);

include("common.php");

function myflush()
{
@ob_flush();
flush();
}

do_header("Creating new project");
do_banner();
myflush();

if (!isset($_REQUEST["key"])) {
   print "<h2>No project key specified.</h2>";
   die();
}

$key= $_REQUEST["key"];

if (strlen($key) != 40 || !preg_match("/^[a-z0-9]*$/", $key)) {
   print "<h2>Invalid key.</h2>";
   die();
}

$PROJECT_DIR = $PROJECTS_PATH."/".$key;
mkdir($PROJECT_DIR);
$PROJECT_PDF = $PROJECT_DIR."/"."upload.pdf";

if (!move_uploaded_file($_FILES['pdffile']['tmp_name'], $PROJECT_PDF)) {
   print "File upload failed.\n";
   die();
}

?>
<h2>Your key</h2>

<P>Your project has been assigned a unique 'key' which will give anyone who knows it access to your project. <B>If you lose this key, you will lose access to your project.</B></P>

<P><b>Your key: </b><?php print $key ?>

<P>You can also bookmark the next page, whose URL contains your key. </P>

<h2>Project creation status</h2>

<?php

print "File uploaded succeeded....<br>\n";
myflush();

print "Splitting PDF into slides...<br>\n";
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
for (i = 0; i < nslides; i++) {
    var slide = new Object();
    slide['type'] = "slide";
    slide['slide'] = slide_names[i];
    slide['thumb'] = slide_names[i].replace("pdfslide", "pdfthumb");
    slide['progress'] = 1;
    slide['seconds'] = Math.floor(5*60 / nslides);

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

print "<a href=project.php?key=$key>Go to project page</a>";

do_footer();
?>
