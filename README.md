# Entity Plus CMI

This module allows configuration entities to be stored as CMI config json files. It also allows
developers to access config files as Backdrop entities.

## Backgroung information

### What are Backdrop config files?

Backdrop introduced json configuration files (CMI) as a way to save and retrieve configuration objects such as
field and field instance definitions, content types (a.k.a. node bundles), taxonomy vocabularies, etc. In Drupal 7, 
these configuration objects were saved in the database. With CMIs, Backdrop introduced the Configuration Manager, which allows for
conveniently synchronizing, importing and exporting configurations through the UI. 

### What are configuration entities?

In Drupal 7, the contrib module Entity API allowed developers to define configuration or exportable custom entities. These configuration entities allowed developers to define bundles for custom entities - they were the equivalent version of content types (node bundle definitions). 

For example, with Entity API, you could create a custom entity type called `costumer` and a configuration entity called `costumer_type` that allowed you to create different bundles of `costumer` entities, such as `domestic_customer` or `foreign_customer`.

These configuration entities (e.g. instances of `customer_type`) were stored in the database.

Other contrib modules used these database-stored configuration entity functionality. One of the most glaring examples is Rules: it stored all rules as configuration entities in the database. 

### Entity Plus CMI and configuration entities.

Backdrop contrib module Entity Plus provides the same functionality as Entity API in Drupal 7. However, with the availability of json config files in Backdrop, it's often desirable to store these configuration entities as json files, instead of in the database. This allows developers to easily import/export these entities using the same Configuration manager UI.

Enters Entity Plus CMI.

This module does exactly that: it allows developers to store configuration entities as CMI json files. All this without losing the benefits of entities. You can still use entity metadata wrappers with these special CMI entities, and you can also use pretty much all API functions like `entity_load` and `entity_create` with these CMI entities, effectively bypassing the Config API (this module wraps many of the Config API functions). 

Additionally, Entity Plus allows you to create metadata for the "properties" of these entities by implementing `hook_entity_property_info()`. This will allow you to use `entity_view()` to display the values stored in the config file as if they were fields of a normal entity.

But there is more! You can also access values of the CMI configuration entity via tokens with `Entity Tokens`!   

(Of course, there are some limitations).

## Installation

- Install this module using the [official Backdrop CMS instructions](https://backdropcms.org/guide/modules)

## Usage

A full example module will be provided as part of this project soon. In the meantime... 

1. Define the entity:

```php
function MYMODULE_entity_info() {
  
  // Definition for the bundle definition entity
  $return['customer_type'] = array(
    'label' => t('Customer type'),
    'entity class' => 'EntityPlusCmiEntity', // Extend this class to create your own uri() and label() etc.
    'controller class' => 'EntityPlusCmiController', // Extend this class to tinker with buildContent() etc.
    'views controller class' => FALSE, // IMPORTANT: These configuration entities do NOT provide views integration. 
    //'base table' => 'customer_type', // `base_table` is ignored
    'fieldable' => FALSE,  // Like in Drupal 7, configuration entities are not fieldable (but they can have as many properties as you wish)
    'bundle of' => 'customer', // Since this configuration entity is a bundle definition for a content entity called `customer`, you need to specify it here. This will tell Field API that this configuration entity is used as a bundle definition for `customer`, and Field API will allow you to attach fields to the different bundles of `customer`.  
    'exportable' => TRUE, 
    'configurable' => TRUE, // IMPORANT! Without this, this API will break!!!
    'static cache' => TRUE, // This allows for static caching of these entities.
    'entity keys' => array( // This array specifies the machine names of the json file properties used as entity keys.
      'id' => 'id', // This will be ignored, but still needs to be provided, since it's required by Entity. CMI entities do not use numeric IDs.
      'name' => 'type', // IMPORTANT. You MUST specify which property of the json file will be used as the machine name for the entity.
      'label' => 'label', // This is the json file property that provides the human label of the entity
      'module' => 'module', // This is the json file property that provides the module that has defined this entity.
    ),
    'module' => 'MYMODULE',
    // Enable the admin UI provided by Entity UI.
    'admin ui' => array(
      'path' => 'admin/structure/customer-types', // This is the path for the UI that allows for creation and editing of these config entities, AND also for adding fields and managing the field display for the bundles these entities define ('bundle of' above is needed for this field functionality).
      'file' => 'MYMODULE.admin.inc', // A place to provide the entity form for creating/editing these entities. This form should be called `customer_type_form`. 
      'controller class' => 'EntityPlusCmiUIController', // This class extends EntityUIController and is needed for CMI entities.
    ),
    'access callback' => 'customer_type_access', // Callback function for access.
    'access arguments' => array('administer customer types'), // This right needs to be provided by hook_permission()
    'extra fields controller class' => 'EntityDefaultExtraFieldsController', // This will tell Entity Plus to provide standard theming for the customer_type entity properties. 
    'token type' => 'customer_type', // This tells Entity Tokens to create tokens based in the properties defined by hook_entity_property_info(). These tokens will be name-spaced as [customer_type:PROPERTY]
  );

  return $return;
}
```

The above info definition corresponds to json files like:
```json
{
    "_config_name": "customer_type.foreign_customer",
    "status": 1,
    "module": "MYMODULE",
    "label": "Foreign customer",
    "type": "foerign_customer",
    "description": "Bundle definition for foreign customer entities.",
    "additional_property_1" : "a value",
    "additional_property_2" : "another value",
}
```

NOTE: the name of the file for the json above will be `customer_type.foreign_customer.json`. This particular entity will define the bundle `foreign_customer` of type `customer`. 


2. Define the entity properties. These are in some way "pseudo fields" and take the place of hook_schema() in defining each of the properties of an entity. Notice that `type` corresponds to the data types defined for Entity Metadata Wrappers. See https://www.drupal.org/docs/7/api/entity-api/data-types

```php
function MYMODULE_entity_property_info_alter(&$info) {
  $properties = &$info['customer_type']['properties'];
  $properties['label'] = array(
    'type' => 'text',
    'label' => t('Label'),
    'description' => t('The human label of the customer type.'),
  );
  $properties['type'] = array(
    'type' => 'token',
    'label' => t('Bundle'),
    'description' => t('The machine name of the configuration entity. This is also used to define the bundle of Customer entities.'),
  );
  $properties['description'] = array(
    'type' => 'text',
    'label' => t('Description'),
    'description' => t('A description.'),
  );
  $properties['additional_property_1'] = array(
    'type' => 'text',
    'label' => t('Additional property 1'),
    'description' => t('An additional property.'),
  );
  $properties['additional_property_2'] = array(
    'type' => 'text',
    'label' => t('Additional property 2'),
    'description' => t('An additional property.'),
  );
}
```

3. Define permissions

```php
function MYMODULE_permission() {
  $permissions = array(
    'administer customer types' => array(
      'title' => t('Administer customer types'),
      'description' => t('Allows users to configure customer types and their fields.'),
      'restrict access' => TRUE,
    ),
  );

  return $permissions;
}

```

4. Provide an access function

```php
/**
 * Access callback for customer_type.
 */
function customer_type_access($op) {
  return user_access('administer customer types');
}
```

5. Provide a form and submit callback to edit the configuration entity. This form should go in `MYMODULE.admin.inc`

```php
function customer_type_form($form, &$form_state, $customer_type, $op = 'edit') {

  if ($op == 'clone') {
    $customer_type->label .= ' (cloned)';
    $customer_type->type = '';
  }

  $form['label'] = array(
    '#title' => t('Label'),
    '#type' => 'textfield',
    '#default_value' => $customer_type->label,
    '#description' => t('The human-readable name of this customer type.'),
    '#required' => TRUE,
    '#size' => 30,
  );

  // Machine-readable type name.
  $form['type'] = array(
    '#type' => 'machine_name',
    '#default_value' => isset($customer_type->type) ? $customer_type->type : '',
    '#maxlength' => 32,
    '#disabled' => $customer_type->isLocked() && $op != 'clone',
    '#machine_name' => array(
      'exists' => 'customer_types',  // IMPORTANT: you must provide the function `customer_types()` that returns all customer_type entities. This function must be placed in the main module.
      'source' => array('label'),
    ),
    '#description' => t('A unique machine-readable name for this customer type. It must only contain lowercase letters, numbers, and underscores.'),
  );

  $form['description'] = array(
    '#type' => 'textarea',
    '#default_value' => isset($customer_type->description) ? $customer_type->description : '',
    '#description' => t('Description of the customer type.'),
  );

  $form['additional_property_1'] = array(
    '#type' => 'textfield',
    '#default_value' => isset($customer_type->additional_property_1) ? $customer_type->additional_property_1 : '',
    '#description' => t('Additional property 1.'),
  );

  $form['additional_property_2'] = array(
    '#type' => 'textfield',
    '#default_value' => isset($customer_type->additional_property_2) ? $customer_type->additional_property_2 : '',
    '#description' => t('Additional property 2.'),
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save customer type'),
    '#weight' => 40,
  );

  if (!$customer_type->isLocked() && $op != 'add' && $op != 'clone') {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete customer type'),
      '#weight' => 45,
      '#limit_validation_errors' => array(),
      '#submit' => array('customer_type_form_submit_delete') // Important: you must provide this submit callback.
    );
  }
  return $form;
}

/**
 * Submit handler for creating/editing customer_type.
 */
function customer_type_form_submit(&$form, &$form_state) {

  $customer_type = $form_state['customer_type'];
  if (empty($customer_type->is_new)) {
    $customer_type->original = clone $customer_type; // It's important to provide an original entity in case the machine name of the entity is changed. These entities have no numeric IDs, and therefore the only way to retrieve the old values is through this clone.
  }

  $values = $form_state['values'];
  $customer_type->type = $values['type'];
  $customer_type->label = $values['label'];
  $customer_type->description = $values['description'];
  // Save and go back.
  entity_plus_save('customer_type', $customer_type);

  // Redirect user back to list of customer types.
  $form_state['redirect'] = 'admin/structure/customer-types';
}
```

## Created and maintained by:
- [argiepiano](https://github.com/argiepiano)
- Seeking co-maintainers


## License

This project is GPL v2 software. See the LICENSE.txt file in this directory
for complete text.
