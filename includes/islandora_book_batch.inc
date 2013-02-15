<?php

/**
 * @file
 * Implementation of classes from the batch ingest framework for books.
 */

/**
 * Batch preprocessor.
 */
class IslandoraBookBatch extends IslandoraScanBatch {
  protected static function get_object_class() {
    return 'IslandoraBookBookBatchObject';
  }

  /**
   * Parses the file listing into the structure we use.
   *
   * @overrides IslandoraScanBatch::group_files()
   */
  protected function group_files($listing) {
    $files = array();

    foreach ($listing as $relative_uri => $file_info) {
      $dir_name = dirname($relative_uri);
      $components = explode(DIRECTORY_SEPARATOR, $dir_name);
      $this->re_group($files, $components, $relative_uri, $file_info);
    }

    // We are not interested in files at the top level:
    unset($files['.']);

    if ($this->parameters['wait_for_metadata']) {
      $metadata_entries = array('MODS', 'DC', '--METADATA--');
      $no_metadata = array();
      foreach ($files as $key => $value) {
        if (count(array_intersect(array_keys($value), $metadata_entries)) === 0) {
          $no_metadata[$key] = $key;
        }
      }

      $files = array_diff_key($files, $no_metadata);
    }


    if ($this->get_target_type() == 'directory' && $this->parameters['directory_dedup']) {
      // Grab all entries starting with the target from the resources table.
      $target = drupal_realpath($this->get_target());
      $target_length = strlen($target);

      $q = db_select('islandora_batch_resources', 'r')
        ->fields('r')
        ->condition('type', 'book_directory')
        ->condition('resource', $target . '%', 'LIKE');

      // Remove entries from the $files array which already exist in the resources table.
      foreach ($q->execute() as $result) {
        // Grab the relative path of the book resource entry.
        $relative_path = substr($result->resource, $target_length + 1);

        // Unset the entry from the array, if it exists.
        if (isset($files[$relative_path])) {
          unset($files[$relative_path]);
        }
      }
    }

    return $files;
  }

  /**
   * Recursively regroup/parse the listing.
   */
  protected function re_group(&$files, $components, $relative_uri, $file_info) {
    $first_component = array_shift($components);
    $count_components = count($components);

    if ($first_component === NULL || $first_component == '.') {
      $files['.'][$file_info->name] = $file_info;
    }
    else {
      if (!isset($files[$first_component])) {
        $files[$first_component] = array(
          '.' => array(),
        );
      }
      $this->re_group($files[$first_component], $components, $relative_uri, $file_info);
    }
  }

  public function preprocess() {
    $added = parent::preprocess();

    $object_type = static::get_object_class();

    foreach ($added as $object) {
      if ($object instanceof $object_type) {
        $children = db_select('islandora_batch_queue', 'q')
          ->fields('q', array('id'))
          ->condition('parent', $object->id);

        // Set pages ready to go.
        db_update('islandora_batch_state')
          ->fields(array(
            'state' => ISLANDORA_BATCH_STATE__READY,
          ))
          ->condition('id', $children, 'IN')
          ->execute();
        // Set book ready to go (after children, that is).
        db_update('islandora_batch_state')
          ->fields(array(
            'state' => ISLANDORA_BATCH_STATE__PENDING_CHILDREN,
          ))
          ->condition('id', $object->id)
          ->execute();
      }
    }

    return $added;
  }
}

/**
 * Each directory is an object. Each contained file is a datastream.
 *
 * Files are used "DSID.ext", and the extension is used to figure out the
 * mimetype for the datastream.
 * @todo Should be moved into islandora_batch.
 */
class IslandoraFlatBatchObject extends IslandoraScanBatchObject {
  public function batch_process() {
    // Use object_info to create some datastreams.
    $this->label = $this->get_title();
    $this->get_mods();
    $this->get_dc();

    // Add other assets...

    // XXX: This should be made into a constant or sommat?
    // Skip MODS and DC (and --METADATA--), 'cause they get handled in other functions.
    $skip = drupal_map_assoc(array('MODS', 'DC', '--METADATA--'));

    foreach (array_diff_key($this->object_info['.'], $skip) as $dsid => $obj) {
      $obj_datastream = $this->constructDatastream($dsid);
      // Figure out the mimetype.
      $obj_datastream->mimetype = static::get_mime($obj);
      $obj_datastream->label = "$dsid Datastream";
      $obj_datastream->setContentFromFile($obj->uri, FALSE);

      $this->ingestDatastream($obj_datastream);
    }

    return ISLANDORA_BATCH_STATE__DONE;
  }

  protected function get_mods() {
    if (!isset($this['MODS'])) {
      $mods_datastream = $this->constructDatastream('MODS', 'X');
      $mods_datastream->mimetype = 'text/xml';
      $mods_datastream->label = 'MODS Record';

      $dir = drupal_get_path('module', 'islandora_batch');

      if (isset($this->object_info['.']['MODS'])) {
        $mods_datastream->setContentFromFile($this->object_info['.']['MODS']->uri, FALSE);
      }
      elseif (isset($this->object_info['.']['--METADATA--'])) {
        // Detect type of contents, and transform.
        $this->get_mods_from_general_source($mods_datastream);
      }
      elseif (isset($this->object_info['.']['DC'])) {
        $mods_datastream->content = static::run_xsl_transform(array(
          'input' => file_get_contents($this->object_info['.']['DC']->uri),
          'xsl' => $dir . '/transforms/simpleDC2MODS.xsl',
        ));
      }

      if (!isset($mods_datastream->content)) {
      // If we do not yet have any XML, create a dumb MODS with just the
      //   title, and set it...
        $title = $this->base_name;

        $mods_datastream->content = <<<EOXML
<mods xmlns:mods="http://www.loc.gov/mods/v3" xmlns="http://www.loc.gov/mods/v3">
  <titleInfo>
    <title>$title</title>
  </titleInfo>
</mods>
EOXML;
      }

      $this->ingestDatastream($mods_datastream);
    }

    return $this['MODS']->content;
  }

  /**
   * Transform from --METADATA--.{mrc,xml} to MODS.
   *
   * @param FedoraDatastream $datastream
   *
   */
  protected function get_mods_from_general_source(AbstractDatastream $datastream) {
    $meta = $this->object_info['.']['--METADATA--'];
    $ext = strtolower(pathinfo($meta->filename, PATHINFO_EXTENSION));
    $dir = drupal_get_path('module', 'islandora_batch');
    if ($ext == 'xml') {
      $xml = file_get_contents($meta->uri);

      if ($xml) {
        $s_xml = new SimpleXMLElement($xml);
    //   MODS, set
        if ($s_xml->getName() == 'mods') {
          // XXX: Should probably create a resource entry.
          $datastream->setContentFromFile($meta->uri, FALSE);
        }
    //   MARCXML, transform to MODS and set
        elseif ($s_xml->getName() == 'record') {
          $datastream->content = static::run_xsl_transform(array(
            'input' => $xml,
            'xsl' => $dir . '/transforms/MARC21slim2MODS3-4.xsl',
          ));
        }
    //   DC, transform to MODS and set
        elseif ($s_xml->getName() == 'dc') {
          $datastream->content = static::run_xsl_transform(array(
            'input' => $xml,
            'xsl' => $dir . '/transforms/simpleDC2MODS.xsl',
          ));
        }
    //   Unrecognized format...
        else {
          // Do nothing?
        }
      }
    }
    elseif ($ext == 'mrc') {
      // Generate some MODS from binary MARC.

      // Get the java executable.
      $java = variable_get('islandora_batch_java', 'java');

      $temp_file_marc = tempnam('/tmp', 'binmrc');
      $temp_file_mods = tempnam('/tmp', 'mods');

      // Copy the file, so we're sure it's accessible on the filesystem.
      copy($meta->uri, $temp_file_marc);
      $absolute = realpath($dir);

      // The command to run.
      $command = "$java -cp $absolute/transforms/marcxml/marcxml.jar:$absolute/transforms/marcxml/marc4j.jar gov.loc.marcxml.MARC2MODS $temp_file_marc $temp_file_mods";

      // Run the command.
      $proc = popen($command, 'r');
      pclose($proc);

      // Load the output into a DOMDocument.
      $doc = new DOMDocument();
      $doc->load($temp_file_mods);

      // Drop the temp.
      unlink($temp_file_marc);
      unlink($temp_file_mods);

      // The transformation produces a collection of mods docs (even if there's
      //   only one)...  Let's grab the first one.
      $mods_docs = $doc->getElementsByTagName('mods');

      if ($mods_docs->length > 0) {
        // Grab the first.
        $mods = new DOMDocument();
        $mods_node = $mods_docs->item(0);
        $mods_node =  $mods->importNode($mods_node, TRUE);
        $mods->appendChild($mods_node);

        // TODO: Transform dates to Solr format...  Or should this be left to
        //   a core hook?

        // And put the content into the datastream.
        $datastream->content = $mods->saveXML();
      }
    }
  }

  protected function get_dc() {
    if (!isset($this['DC'])) {
      // Create the DC datastream on this object.
      $dc_datastream = $this->constructDatastream('DC', 'X');
      $dc_datastream->mimetype = 'text/xml';
      $dc_datastream->label = 'DC Record';

      if (isset($this->object_info['.']['DC'])) {
        $dc_datastream->setContentFromFile($this->object_info['.']['DC']->uri, FALSE);
      }
      elseif (isset($this->object_info['.']['--METADATA--'])) {
        // If --METADATA-- contains DC directly, use it.
        $this->get_dc_from_general_source($dc_datastream);
      }

      // Get the DC by transforming from MODS.
      if (!isset($dc_datastream->content)) {
        // XXX: Might want to make this use a file, instead of staging the DC
        //   in the database table (inside the object we serialize).
        $new_dc = static::run_xsl_transform(array(
          'xsl' => drupal_get_path('module', 'islandora_batch') . '/transforms/mods_to_dc.xsl',
          'input' => $this->get_mods(),
        ));
        if ($new_dc) {
          $dc_datastream->content = $new_dc;
        }
      }

      if (isset($dc_datastream->content)) {
        $this->ingestDatastream($dc_datastream);
      }
    }

    return $this['DC']->content;
  }

  /**
   * If --METADATA--.xml happens to be DC, grab it...
   *
   * ... otherwise, we'll let the MODS implementation transform it.
   *
   * @param FedoraDatastream $datastream
   *   The datastream to which we should add the metadata.
   */
  protected function get_dc_from_general_source(AbstractDatastream $datastream) {
    $meta = $this->object_info['.']['--METADATA--'];

    if ('xml' == pathinfo($meta->filename, PATHINFO_EXTENSION)) {
      $xml = file_get_contents($meta->uri);

      if ($xml) {
        $s_xml = new SimpleXMLElement($xml);
        if ($s_xml->getName() == 'dc') {
          // XXX: Should probably create a resource entry.
          $dc_datastream->setContentFromFile($meta->uri, FALSE);
        }
      }
    }
  }
}

class IslandoraBookBookBatchObject extends IslandoraFlatBatchObject {
  public function __construct(IslandoraTuque $connection, $base_name, $object_info, $pp) {
    parent::__construct($connection, $base_name, $object_info, $pp);

    if ($pp['directory_dedup'] && $pp['type'] == 'directory') {
      $this->resources['book_directory'] = array(
        drupal_realpath($pp['target'] . '/' . $this->base_name),
      );
    }
  }

  public function initial_batch_state() {
    // Not ready until we've added the pages.
    return ISLANDORA_BATCH_STATE__NOT_READY;
  }

  public function get_children(IslandoraTuque $connection) {
    // Create page objects, return in an array.
    $children = array();

    foreach ($this->object_info as $sequence => $info) {
      if ($sequence != '.') {
        $children[] = new IslandoraBookPageBatchObject($connection, $this->id, $sequence, $info, $this->preprocessor_parameters);
      }
    }

    return $children;
  }

  public function add_relationships() {
    parent::add_relationships();

    if ($this->preprocessor_parameters['create_pdfs']) {
      $this->relationships->add(ISLANDORA_RELS_EXT_URI, 'create_pdf', 'true', TRUE);
    }

    if ($this->preprocessor_parameters['email_admin']) {
      $this->relationships->add(ISLANDORA_RELS_EXT_URI, 'email-admin-when-ingested', 'true', TRUE);
    }
  }
}

class IslandoraBookPageBatchObject extends IslandoraFlatBatchObject {
  protected $parent_id, $sequence_number;

  public function __construct(IslandoraTuque $connection, $parent_id, $sequence, $object_info, $pp) {
    parent::__construct($connection, "$parent_id-$sequence", $object_info, $pp);

    $this->parent_id = $parent_id;
    $this->sequence_number = $sequence;
  }

  public function get_children(IslandoraTuque $connection) {
    // Pages should not have children.
    return array();
  }

  public function initial_batch_state() {
    // Not ready.
    return ISLANDORA_BATCH_STATE__NOT_READY;
  }

  public function add_relationships() {
    module_load_include('inc', 'islandora_book', 'includes/utilities');
    // Add relationship to collection.
    $rels_ext = $this->relationships;
    islandora_book_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageOf', $this->parent_id);
    islandora_book_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSequenceNumber', (string) $this->sequence_number, TRUE);
    islandora_book_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageNumber', (string) $this->sequence_number, TRUE);
    islandora_book_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSection', '1', TRUE);
    islandora_book_set_relationship($rels_ext, FEDORA_RELS_EXT_URI, 'isMemberOf', $this->parent_id);
    // Add content model relationship.
    $this->models = 'islandora:pageCModel';
  }
}

