<?php 

/**
 * @file
 * Defines entity and controller classes for CMI configuration entities.
 */

interface EntityPlusCmiInterface extends EntityInterface {
   /**
   * Set value for a property.
   * 
   * @param string $property
   *   The property name.
   * 
   * @param $value
   *   The value to set.
   */
  public function setProperty($property, $value);

  /** 
   * Get a property value.
   * 
   * @param string $property
   *   The name of the property to get.
   */
  public function getProperty($property);
}

/** 
 * Entity class for CMI configuration entities.
 */
class EntityPlusCmiEntity extends Entity implements EntityPlusCmiInterface {
 
  protected $entityType;
  protected $entityInfo;
  protected $nameKey;
  protected $idKey;
  protected $statusKey;
  protected $labelKey;
  protected $moduleKey;

  function __construct(array $config_values = array(), $entity_type = NULL) {
    $this->entityType = $entity_type;
    $this->entityInfo = entity_get_info($entity_type);
    $this->nameKey = isset($this->entityInfo['entity keys']['name']) ? $this->entityInfo['entity keys']['name'] : 'name';
    $this->idKey = $this->nameKey;
    $this->statusKey = isset($this->entityInfo['entity keys']['status']) ? $this->entityInfo['entity keys']['status'] : 'status';
    $this->labelKey = isset($this->entityInfo['entity keys']['label']) ? $this->entityInfo['entity keys']['label'] : 'label';
    $this->moduleKey = isset($this->entityInfo['entity keys']['module']) ? $this->entityInfo['entity keys']['module'] : 'module';
    $this->defaultValues();
    parent::__construct($config_values);
  }

  /**
   * Return the entity type for this entity.
   * 
   * @return string 
   *   The entity type.
   */
  function entityType() {
    return $this->entityType;
  }

  /**
   * Return the default values for "fields".
   * 
   * Since these entities don't have a schema, we create default "fields" here.
   * These defaults can be overriden by children entity classes. However, these
   * entities should always have at least a 'name' key, a 'module' key, a 
   * 'status' key and a 'label' key.   
   */
  function defaultValues() {
    $this->{$this->statusKey} = ENTITY_CUSTOM;
    $this->{$this->moduleKey} = isset($this->entityInfo['module']) ? $this->entityInfo['module'] : $this->entityType;
    $this->{$this->labelKey} = 'label';
  }

  /**
   * Override EntityInterface.
   */
  function label() {
    return $this->{$this->labelKey};
  }

  /**
   * Override EntityInterface.
   */
  public function id() {
    // Since this is a configuration entity loaded by machine name rather than
    // numeric id, we return the key holding the 'name' property. 
    return $this->{$this->nameKey};
  }

  /**
   * Override EntityInterface.
   * 
   * This method should be overridden if you want to provide a uri to view this
   * config entity.
   */
  public  function uri() {}

  /**
   * Check if the entity is locked.
   * 
   * @return bool
   *   A boolean indicating if the entity is locked.
   */ 
  public function isLocked() {
    return isset($this->{$this->statusKey}) && empty($this->is_new) && (($this->{$this->statusKey} & ENTITY_IN_CODE) || ($this->{$this->statusKey} & ENTITY_FIXED));
  }

  /** 
   * Get a property value.
   * 
   * @param string $property
   *   The name of the property to get.
   */
  public function getProperty($property) {
    return isset($this->$property) ? $this->$property : NULL;
  }

  /**
   * Set value for a property.
   * 
   * @param string $property
   *   The property name.
   * 
   * @param $value
   *   The value to set.
   */
  public function setProperty($property, $value) {
    $this->$property = $value;
  }

  /** 
   * Save the entity.
   */
  public function save() {
    return entity_get_controller($this->entityType)->save($this);
  }
}

/**
 * Controller for configuration entities stored as CMI files. 
 */
class EntityPlusCmiController extends DefaultEntityController implements EntityControllerInterface {

  protected $entityType;
  protected $entityInfo;
  protected $nameKey;
  protected $idKey;
  protected $statusKey;
  protected $moduleKey;
  protected $entityCache;
  protected $staticCache;
  protected $persistentCache;
  protected $bundleKey;

  function  __construct($entity_type) {
    $this->entityType = $entity_type;
    $this->entityInfo = entity_get_info($entity_type);
    $this->entityCache = array();
    if (isset($this->entityInfo['bundle of'])) {
      $info = entity_get_info($this->entityInfo['bundle of']);
      $this->bundleKey = $info['bundle keys']['bundle'];
    }
    $this->nameKey = isset($this->entityInfo['entity keys']['name']) ? $this->entityInfo['entity keys']['name'] : 'name';
    // Since these entities don't use numeric IDs, the idKey is the same as the nameKey.
    $this->idKey = $this->nameKey;
    $this->statusKey = isset($this->entityInfo['entity keys']['status']) ? $this->entityInfo['entity keys']['status'] : 'status';
    $this->moduleKey = isset($this->entityInfo['entity keys']['module']) ? $this->entityInfo['entity keys']['module'] : 'module';
    $this->staticCache = !empty($this->entityInfo['static cache']);
  }

  /**
   * Loads entities.
   * 
   * @param array $names
   *   The machine names or the entities to be retrieve, or FALSE or array() to
   *   retrieve all available entities of the type stored in $this->entityType.
   * 
   * @return array
   *   An array of entities keyed by machine name.
   * 
   * @todo Integrate static cache.
   */
  public function load($names = array(), $conditions = array()) {
    $entities = array();

    if (empty($names)) {
      $names = $this->getAllNames();
    }

    if ($this->staticCache) {
      $entities += $this->cacheGet($names, $conditions);
      // Remove names that have been loaded from the static cache.
      $names = array_keys(array_diff_key(array_flip($names), $entities));
    }

    $retrieved_entities = array();
    foreach ($names as $name) {
      $file_name = $this->entityType . '.' . $name;
      $config = config($file_name);
      if (!$config->isNew()) {
        $config_values = $config->get();
        $class = $this->entityInfo['entity class'];
        $retrieved_entities[$name] = new $class($config_values, $this->entityType);  
      }
    }

    $entities += $retrieved_entities;

    // Exclude any entities if they don't match $conditions.
    // This ensures the same behavior whether loading from memory or database.
    if ($conditions) {
      foreach ($retrieved_entities as $entity) {
        // Iterate over all conditions and compare them to the entity
        // properties. We cannot use array_diff_assoc() here since the
        // conditions can be nested arrays, too.
        foreach ($conditions as $property_name => $condition) {
          if (is_array($condition)) {
            // Multiple condition values for one property are treated as OR
            // operation: only if the value is not at all in the condition array
            // we remove the entity.
            if (!in_array($entity->{$property_name}, $condition)) {
              unset($entities[$entity->{$this->nameKey}]);
              continue 2;
            }
          }
          elseif ($condition != $entity->{$property_name}) {
            unset($entities[$entity->{$this->nameKey}]);
            continue 2;
          }
        }
      }
    }

    if ($this->staticCache) {
      // Add entities to the cache.
        $this->cacheSet($retrieved_entities);
    }

    return $entities;
  }

  /** 
   * Creates an entity.
   * 
   * @param array $values
   *   Values to initialize the entity with.
   * 
   * @return EntityInterface
   *   An initialized entity. 
   */
  function create(array $values = array()) {
    // Add is_new property if it is not set.
    $values += array('is_new' => TRUE);
    if (isset($this->entityInfo['entity class']) && $class = $this->entityInfo['entity class']) {
      return new $class($values, $this->entityType);
    }
    return (object) $values;
  }

  /** 
   * Saves an entity.
   * 
   * @param EntityInterface $entity
   *   The entity to be saved.
   * 
   * @return Config
   *   The Config object associated with the entity.
   * 
   */
  function save($entity) {
    try {
      // Preload $entity->original by name key if necessary.
      if (empty($entity->is_new)) {
        if (!isset($entity->original)) {
          $original = $this->load(array($entity->{$this->nameKey}));
          $original = reset($original);
        } 
        else {
          $original = $entity->original;
          // Temporarily unset to avoid saving the original.
          unset($entity->original);
        }

        $diff = array_diff_key((array) $original, (array) $entity);
      }

      $file_name = $this->entityType . '.' . $entity->{$this->nameKey};

      $this->invoke('presave', $entity);

      $is_new = !empty($entity->is_new);
      unset($entity->is_new);
      unset($entity->is_active_revision);

      $config = config($file_name);
      foreach ($entity as $key => $value) {
        $config->set($key, $value); 
      }

      // Remove deleted properties.
      if (!empty($diff)) {
        foreach (array_keys($diff) as $key) {
          $config->clear($key);
        }
      }
      // Save the config file.
      $return = $config->save();

      // Prepare the entity for hooks.
      if (isset($original)) {
        $entity->original = $original;
      }

      // Invoke hooks.
      if ($is_new) {
        if (!empty($this->entityInfo['bundle of'])) {
          field_attach_create_bundle($this->entityInfo['bundle of'], $entity->{$this->nameKey});
        }
        $this->invoke('insert', $entity);
      }
      else {
        $this->invoke('update', $entity);
      }

      // If the machine name has changed for an existing entity, delete the 
      // configuration file and update the fields.
      if (isset($entity->original) && $entity->original->{$this->nameKey} != $entity->{$this->nameKey}) {
        config($this->entityType . '.' . $entity->original->{$this->nameKey})->delete();
        if (!empty($this->entityInfo['bundle of'])) {
          field_attach_rename_bundle($this->entityInfo['bundle of'], $entity->original->{$this->nameKey}, $entity->{$this->nameKey});
        }
      }

      unset($entity->original);

      return $return;
    }

    catch (Exception $e) {
      watchdog_exception($this->entityType, $e);
      throw $e;
    }
  }

  /**
   * Delete entities.
   * 
   * @param array $names
   *   The machine names of the entities to be deleted.
   * 
   * @return Config
   *   The Config object associated with the entity.
   */
  public function delete(array $names) {
    $entities = $this->load($names);
    if (empty($entities)) {
      // Do nothing if there are no entities to delete.
      return;
    }
    foreach ($names as $name) {
      $file_name = $this->entityType . '.' . $name;
      $config = config($file_name);
      return $config->delete();
    }
    foreach ($entities as $entity) {
      $this->invoke('delete', $entity);
    }
  }

  /** 
   * Invokes hooks.
   * 
   * @param string $hook
   *   The hook name, as in 'update' or 'insert'.
   * 
   * @param EntityInterface $entity
   *   The entity passed to the hooks. 
   */
  public function invoke($hook, $entity) {
    // Invoke the hook.
    module_invoke_all($this->entityType . '_' . $hook, $entity);
    // Invoke the respective entity level hook.
    if ($hook == 'presave' || $hook == 'insert' || $hook == 'update' || $hook == 'delete') {
      if (is_a($entity, 'Entity')) {
        // Invoke all regular entity hooks as well as entity_plus hooks.
        // See https://github.com/backdrop-contrib/entity_plus/issues/35
        module_invoke_all('entity_' . $hook, $entity, $this->entityType);
        module_invoke_all('entity_plus_' . $hook, $entity, $this->entityType);
      }
    }
    // Invoke rules.
    if (module_exists('rules')) {
      rules_invoke_event($this->entityType . '_' . $hook, $entity);
    }
  }

  /** 
   * Helper to get all machine names of entities of this type.
   * 
   * @return array 
   *   An array of the machine names of all entities of this type.
   */
  protected function getAllNames() {
    $all_files = config_get_names_with_prefix($this->entityType . '.');
    $names = array();
    foreach ($all_files as $file) {
      $names[] = str_replace($this->entityType . '.', '', $file);
    }
    return $names;
  }

  public function buildContent(EntityInterface $entity, $view_mode = 'full', $langcode = NULL, $content = array()) {
    global $language_content;
    $langcode = $langcode ? $langcode : $language_content->langcode;

    // Remove previously built content, if exists.
    $entity->content = array();

    // Allow modules to change the display mode.
    $view_mode = key(entity_view_mode_prepare($entity->entityType(), array($entity->id() => $entity), $view_mode, $langcode));

    // Make sure the used view-mode gets stored.
    $entity->content += array('#view_mode' => $view_mode);

    // Add properties only if $info['extra fields controller class'] is defined.
    if ($extra_field_controller = entity_plus_get_extra_fields_controller($this->entityType)) {
      $wrapper = entity_metadata_wrapper($this->entityType, $entity);
      $extra = $extra_field_controller->fieldExtraFields();
      $type_extra = &$extra[$this->entityType][$this->entityType]['display'];

      foreach ($wrapper as $name => $property) {
        if (isset($type_extra[$name]) || isset($bundle_extra[$name])) {
          $this->renderEntityProperty($wrapper, $name, $property, $view_mode, $langcode, $entity->content);
        }
      }
    }
    // Invoke hook_ENTITY_view() to allow modules to add their additions.
    module_invoke_all($this->entityType . '_view', $entity, $view_mode, $langcode);

    module_invoke_all('entity_view', $entity, $this->entityType, $view_mode, $langcode);

    return $entity->content;
  }

  public function view($entities, $view_mode = 'full', $langcode = NULL, $page = NULL){
    global $language_content;
    $langcode = $langcode ? $langcode : $language_content->langcode;

    // For Field API and entity_plus_prepare_view, the entities have to be
    // keyed by id.
    $entities = entity_plus_key_array_by_property($entities, $this->idKey);

    entity_prepare_view($this->entityType, $entities);
    $langcode = isset($langcode) ? $langcode : $langcode;

    $view = array();
    foreach ($entities as $entity) {
      $build =$this->buildContent($entity, $view_mode, $langcode);
      $build += array(
        // If the entity type provides an implementation, use this instead the
        // generic one.
        // @see template_preprocess_entity()
        '#theme' => 'entity_plus',
        '#entity_plus_type' => $this->entityType,
        '#entity' => $entity,
        '#view_mode' => $view_mode,
        '#language' => $langcode,
        '#page' => $page,
      );
      // Allow modules to modify the structured entity.
      backdrop_alter(array($this->entityType . '_view', 'entity_plus_view'), $build, $this->entityType);
      $key = isset($entity->{$this->idKey}) ? $entity->{$this->idKey} : NULL;
      $view[$this->entityType][$key] = $build;
    }
    return $view;
  }


  /**
   * Creates a render array for a single entity property.
   */
  protected function renderEntityProperty($wrapper, $name, $property, $view_mode, $langcode, &$content) {
    $info = $property->info();

    $content[$name] = array(
      // Options are 'above', 'inline', 'hidden'.
      '#label_display' => 'above',
      '#label' => $info['label'],
      '#entity_plus_wrapped' => $wrapper,
      '#theme' => 'entity_plus_property',
      '#property_name' => $name,
      '#access' => $property->access('view'),
      '#entity_type' => $this->entityType,
      // Hide display for empty properties.
      '#access' => !(empty($property->value())),
    );
    $content['#attached']['css']['entity_plus.theme'] = backdrop_get_path('module', 'entity_plus') . '/theme/entity_plus.theme.css';
  }
}


/**
 * An Entity UI class for entities stored as CMI files. 
 */
class EntityPlusCmiUIController extends EntityDefaultUIController {

  /**
   * Overrides EntityDefaultUIControler.
   */
  public function overviewTable($conditions = array()) {
   
    $ids = array();

    foreach ( config_get_names_with_prefix($this->entityType . '.') as $file_name) {
      $names[] = str_replace($this->entityType . '.', '', $file_name);
    }

    $entities = $names ? entity_load($this->entityType, $names) : array();
    ksort($entities);

    $rows = array();
    foreach ($entities as $entity) {
      $rows[] = $this->overviewTableRow($conditions, entity_ui_id($this->entityType, $entity), $entity);
    }

    $render = array(
      '#theme' => 'table',
      '#header' => $this->overviewTableHeaders($conditions, $rows),
      '#rows' => $rows,
      '#empty' => t('None.'),
    );
    return $render;
  }
}
