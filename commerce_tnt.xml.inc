<?php

/**
 * @file
 * Handles XML-related stuff for Commerce TNT module.
 */

/**
 * This builds the XML to submit to TNT for rates.
 *
 */
function commerce_tnt_build_rate_request($order) {

  //dsm($order);
  $shipping_details = field_get_items('commerce_order', $order, 'commerce_customer_shipping');
  $shipping_profile = commerce_customer_profile_load($shipping_details[0]['profile_id']);
  $address_info = field_get_items('commerce_customer_profile', $shipping_profile, 'commerce_customer_address');

  foreach (_commerce_tnt_get_cart_products($order) as $product_id) {
    // Load the TNT Shipping Info using the product_id
    $query = db_select('commerce_tnt', 'ct');
    $query->fields('ct',
      array(
        'active',
        'nid',
        'product_id',
        'container_type',
        'number_of_packages',
        'total_weight',
        'length',
        'width',
        'height'
      )
    );
    $query->condition('ct.product_id', $product_id);
    $result = $query->execute();

    if ($result) {
      foreach ($result as $row) {
        $shipping_info[] = $row;
      }
    }
  }
  
  if (!empty($shipping_info)) {
    $request = new SimpleXMLElement('<enquiry/>');
    $request->addAttribute('xmlns', 'http://www.tntexpress.com.au');
  
    $rated = $request->addChild('ratedTransitTimeEnquiry');
  
    $cutOff = $rated->addChild('cutOffTimeEnquiry');
  
    $col = $cutOff->addChild('collectionAddress');
    $col->addChild('suburb', 'Osborne Park');
    $col->addChild('postCode', '6017');
    $col->addChild('state', 'WA');
  
    $del = $cutOff->addChild('deliveryAddress');
    $del->addChild('suburb', $address_info[0]['locality']);
    $del->addChild('postCode', $address_info[0]['postal_code']);
    $del->addChild('state', $address_info[0]['administrative_area']);
  
    $cutOff->addChild('shippingDate', _commerce_tnt_first_available_weekday());
    $cutOff->addChild('userCurrentLocalDateTime', date('c'));
  
    $dangerous = $cutOff->addChild('dangerousGoods');
    $dangerous->addChild('dangerous', 'false');
  
    $lines = $cutOff->addChild('packageLines');
    $lines->addAttribute('packageType', 'N');
  
    foreach ($shipping_info as $shipping) {
  
      $line = $lines->addChild('packageLine');
      $line->addChild('numberOfPackages', $shipping->number_of_packages);
  
      $dims = $line->addChild('dimensions');
      $dims->addAttribute('unit', 'cm');
      $dims->addChild('length', $shipping->length);
      $dims->addChild('width', $shipping->width);
      $dims->addChild('height', $shipping->height);
  
      $weight = $line->addChild('weight');
      $weight->addAttribute('unit', 'kg');
      $weight->addChild('weight', $shipping->total_weight);
  
    }
  
    $payment = $rated->addChild('termsOfPayment');
    $payment->addChild('senderAccount', variable_get('commerce_tnt_account_number', ''));
    $payment->addChild('payer', 'S');
  
    return array(
      'username' => variable_get('commerce_tnt_user_id', ''),
      'password' => variable_get('commerce_tnt_password', ''),
      'version' => 2,
      'xmlRequest' => $request->asXML(),
    );
  }
  else {
    return array();
  }
}


/**
 * Submits an API request to TNT.
 *
 * @param $xml
 *   An XML string to submit to TNT.
 */
function commerce_tnt_api_request($data) {

  if (empty($data)) {
    return FALSE;
  }

  $headers = array('Content-Type: application/x-www-form-urlencoded');
  $request = http_build_query($data, '', '&');
  $url = variable_get('commerce_tnt_post_url', '');

  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_POST, 1);
  curl_setopt($ch, CURLOPT_TIMEOUT, 30);
  curl_setopt($ch, CURLOPT_POSTFIELDS, $request);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
  curl_setopt($ch, CURLOPT_HTTPHEADER, $headers);
  
  $result = curl_exec($ch);
  
  // Log any errors to the watchdog.
  if ($error = curl_error($ch)) {
    watchdog('commerce_tnt', 'cURL error: @error', array('@error' => $error), WATCHDOG_ERROR);
    return FALSE;
  }
  curl_close($ch);

  // If we received data back from the server...
  if (!empty($result)) {
    // Extract the result into an XML response object.
    $response = new SimpleXMLElement($result);
    return $response;
  }
  else {
    return FALSE;
  }
}


/**
 * A helper function to get the first available weekdays date
 *
 * Note: This does not take public holidays into account.
 */
function _commerce_tnt_first_available_weekday() {
  return date('Y-m-d', strtotime("+1 Weekday"));
}