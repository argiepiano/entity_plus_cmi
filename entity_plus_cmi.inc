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
    $this->{$this->statusKey} = ENTITY_CUSTOM;
    $this->{$this->moduleKey} = isset($this->entityInfo['module']) ? $this->entityInfo['module'] : $this->entityType;
    $this->{$this->labelKey} = '';
    $config_values += $this->defaultValues();
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
   * 
   * @return array
   *  An array of default values keyed by property name.
   */
  function defaultValues() {
    return array();
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
    return entity_get_controller($this->entityType())->save($this);
  }
}

/**
 * Controller for configuration entities stored as CMI files. 
 */
class EntityPlusCmiController extends EntityPlusControllerExportable implements EntityControllerInterface {

  function  __construct($entity_type) {
    parent::__construct($entity_type);
    // Default to 'name' if no name key is provided by the entity definition.
    $this->nameKey = isset($this->entityInfo['entity keys']['name']) ? $this->entityInfo['entity keys']['name'] : $this->idKey;
    // Since these entities don't use numeric IDs, the idKey should be the same as the nameKey.
    $this->idKey = $this->nameKey;
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

    // If $names is FALSE get the names of all config files for the entity type.
    if ($names === FALSE) {
      $names = $this->getAllNames();
    }

    if ($this->staticCache) {
      $entities += $this->cacheGet($names, $conditions);
      // Remove names that have been loaded from the static cache.
      $names = array_keys(array_diff_key(array_flip($names), $entities));
    }

    // Load any remaining entities from config.
    $retrieved_entities = array();
    foreach ($names as $name) {
      $file_name = $this->getFileName($name);
      $config = config($file_name);
      if (!$config->isNew()) {
        $config_values = $config->get();
        $class = $this->entityInfo['entity class'];
        $retrieved_entities[$name] = new $class($config_values, $this->entityType);  
      }
    }
    // Exclude any retrieved entities if they don't match $conditions. Unlike
    // entities stored in the DB, we must run applyConditions() to prune out
    // entities that don't match the conditions.
    if (!empty($retrieved_entities)) {
      $retrieved_entities = $this->applyConditions($retrieved_entities, $conditions);
    }

    // Pass all entities retrieved from disk through $this->attachLoad(),
    // which calls the entity type load callback.
    // We use if again because applyConditions above modifies $retrieved_entities.
    if (!empty($retrieved_entities)) {
      $this->attachLoad($retrieved_entities);
      $entities += $retrieved_entities;
      if ($this->staticCache) {
        // Add entities to the cache.
          $this->cacheSet($retrieved_entities);
      }
    }

    return $entities;
  }

  /** 
   * Saves an entity.
   * 
   * @param EntityInterface $entity
   *    The entity to be saved.
   * @param DatabaseTransaction $transaction
   *    This is here for compatibility with 
   *    EntityPlusControllerExportable::save(). This argument is never used
   *    here.  
   * @return Config
   *   The Config object associated with the entity.
   * 
   */
  public function save($entity, DatabaseTransaction $transaction = NULL) {
    try {
      // Preload $entity->original by name key if necessary.
      if (empty($entity->is_new)) {
        if (!isset($entity->original)) {
          // Remove the entity from the static cache.
          unset($this->entityCache[$entity->id()]);
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

      $file_name = $this->getFileName($entity->{$this->nameKey});

      $this->invoke('presave', $entity);

      $is_new = !empty($entity->is_new);
      unset($entity->is_new);
      unset($entity->is_active_revision);

      $config = config($file_name);

      // Update the status for entities getting overridden.
      if (entity_plus_has_status($this->entityType, $entity, ENTITY_PLUS_IN_CODE) && empty($entity->is_rebuild)) {
        $entity->{$this->statusKey} |= ENTITY_PLUS_CUSTOM;
      }

      // Set the required properties, even if they are not defined in
      // hook_entity_property_info(). These are common to all types.
      $config->set($this->statusKey, $entity->{$this->statusKey});
      $config->set($this->nameKey, $entity->{$this->nameKey});
      $config->set($this->moduleKey, $entity->{$this->moduleKey});

      // Get the list of defined properties. All properties must be defined
      // by implements hook_entity_property_info(). 
      $properties = entity_plus_get_property_info($this->entityType);
      foreach ($properties['properties'] as $property_name => $property) {
        if (isset($entity->$property_name)) {
          $config->set($property_name, $entity->$property_name); 
        }
      }

      // Remove deleted properties.
      if (!empty($diff)) {
        foreach (array_keys($diff) as $key) {
          $config->clear($key);
        }
      }
      // Save the config file.
      $config->save();
      $return = $is_new ? SAVED_NEW : SAVED_UPDATED;

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
      // old configuration file.
      if (!empty($entity->original) && $entity->original->{$this->nameKey} != $entity->{$this->nameKey}) {
        config($this->entityType . '.' . $entity->original->{$this->nameKey})->delete();
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
   * @param DatabaseTransaction $transaction
   *    This is here for compatibility with 
   *    EntityPlusControllerExportable::delete(). This argument is never used
   *    here. 
   * 
   * @return Config
   *   The Config object associated with the entity.
   */
  public function delete($names, DatabaseTransaction $transaction = NULL) {
    $entities = $this->load($names);
    if (empty($entities)) {
      // Do nothing if there are no entities to delete.
      return;
    }
    foreach ($names as $name) {
      $file_name = $this->getFileName($name);
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
    parent::invoke($hook, $entity);
    // Since EntityPlusControllerExportable::invoke() doesn't actually invoke
    // the 'hook_entity_HOOK' hooks (a bug), add it here.
    if ($hook == 'presave' || $hook == 'insert' || $hook == 'update' || $hook == 'delete') {
      if (is_a($entity, 'Entity')) {
        module_invoke_all('entity_' . $hook, $entity, $this->entityType);
      }
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

  /**
   * Build the config file name. 
   * 
   * This method can be opverridden to use a different file name pattern. NB:
   * Changing the pattern will necessitate writing a new hook_config_info() so
   * that Backdrop detects the stored config as entities.
   * 
   * @return string
   *  The config file name without the .json extension. 
   */
  protected function getFileName($entity_name) {
    return $this->entityType . '.' . $entity_name;
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
    $names = array();
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
