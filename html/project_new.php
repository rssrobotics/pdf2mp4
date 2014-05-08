<?php

include("common.php");

do_header("New Project");
do_banner();

$key = sha1(rand().rand().rand().rand().rand().rand());
?>

<h1>Step one: upload PDF</h1>
<form enctype="multipart/form-data" method="post" action=project_new_submit.php>
<input type=file name=pdffile><br>
<input type=submit>
<input type=hidden name="key" value="<?php echo $key ?>">
</form>

<?php do_footer(); ?>
