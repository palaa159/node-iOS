// http://developer.apple.com/library/ios/documentation/AddressBook/Reference/ABAddressBookRef_iPhoneOS/Reference/reference.html
// http://developer.apple.com/library/ios/documentation/AddressBook/Reference/ABRecordRef_iPhoneOS/Reference/reference.html
// http://developer.apple.com/library/ios/documentation/AddressBook/Reference/ABPersonRef_iPhoneOS/Reference/reference.html
// http://developer.apple.com/library/ios/documentation/AddressBook/Reference/ABGroupRef_iPhoneOS/Reference/reference.html

#import "addressBook.h"
#include <stdio.h>

using namespace node;
using namespace v8;


// Set up the exports for AddressBook
void AddressBook::Init(v8::Handle<Object> target) {
  HandleScope scope;
  Local<Object> ab = Object::New();
  NODE_SET_METHOD(ab, "getContacts", AddressBook::GetContacts);
  NODE_SET_METHOD(ab, "getGroups", AddressBook::GetGroups);
  target->Set(String::NewSymbol("AddressBook"), ab);
}


// 'createNotification' begins the async notification process. The user should pass
// an "options" Object and an optional callback function (to examine the results of
// the notification).
v8::Handle<Value> AddressBook::GetContacts(const Arguments& args) {
  HandleScope scope;

  // This is the struct that gets passed around EIO
  struct async_request* ar = (struct async_request*) malloc(sizeof(struct async_request));

  // TODO: Add support for the search predicate
  //Local<Object> options = args[0]->ToObject();

  // A callback function is optional (though why would you call
  // this if you didn't want the results?)
  ar->hasCb = false;
  int argsLen = args.Length();
  if (argsLen >= 1) {
    Local<Function> cb = Local<Function>::Cast(args[argsLen-1]);
    ar->cb = Persistent<Function>::New(cb);
    ar->hasCb = true;
  }

  eio_custom(GetContacts_DoRequest, EIO_PRI_DEFAULT, GetContacts_AfterResponse, ar);
  ev_ref(EV_DEFAULT_UC);

  return Undefined();
}

int GetContacts_DoRequest (eio_req * req) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  struct async_request* ar = (struct async_request*)req->data;

  ABAddressBookRef addressBook = ABAddressBookCreate();
  CFArrayRef people = ABAddressBookCopyArrayOfAllPeople(addressBook);
  // TODO: Sort by the user's current sort preference by default, or a configurable sort
  CFIndex count = CFArrayGetCount(people);
  ar->resultsCount = count;
  ar->results = new Record *[count];

  for (CFIndex i=0; i<count; i++) {
    Contact *p = new Contact;
    ABRecordRef pRef = CFArrayGetValueAtIndex(people, i);

    // RecordID
    p->recordId = ABRecordGetRecordID(pRef);

    // FirstName
    NSString* firstNameStr = (NSString *)ABRecordCopyValue(pRef, kABPersonFirstNameProperty);
    p->firstName = firstNameStr != NULL ? [firstNameStr UTF8String] : NULL;
    // MiddleName
    NSString* middleNameStr = (NSString *)ABRecordCopyValue(pRef, kABPersonMiddleNameProperty);
    p->middleName = middleNameStr != NULL ? [middleNameStr UTF8String] : NULL;
    // LastName
    NSString *lastNameStr = (NSString *)ABRecordCopyValue(pRef, kABPersonLastNameProperty);
    p->lastName = lastNameStr != NULL ? [lastNameStr UTF8String] : NULL;
    // Organization
    NSString *organizationStr = (NSString *)ABRecordCopyValue(pRef, kABPersonOrganizationProperty);
    p->organization = organizationStr != NULL ? [organizationStr UTF8String] : NULL;
    // JobTitle
    NSString *jobTitleStr = (NSString *)ABRecordCopyValue(pRef, kABPersonJobTitleProperty);
    p->jobTitle = jobTitleStr != NULL ? [jobTitleStr UTF8String] : NULL;
    // Department
    NSString *departmentStr = (NSString *)ABRecordCopyValue(pRef, kABPersonDepartmentProperty);
    p->department = departmentStr != NULL ? [departmentStr UTF8String] : NULL;

    // PhoneNumbers
    ABMultiValueRef numbers = ABRecordCopyValue(pRef, kABPersonPhoneProperty);
    p->numNumbers = ABMultiValueGetCount(numbers);
    p->numbersNames = new const char *[p->numNumbers];
    p->numbersValues = new const char *[p->numNumbers];
    for (CFIndex j=0; j < p->numNumbers; j++) {
      NSString *numberName = (NSString *)ABMultiValueCopyLabelAtIndex(numbers, j);
      NSString *numberValue = (NSString *)ABMultiValueCopyValueAtIndex(numbers, j);
      if ([numberName hasPrefix:@"_$!<" ])
        numberName = [numberName substringFromIndex:4 ];
      if ([numberName hasSuffix:@">!$_" ])
        numberName = [numberName substringToIndex: [numberName length] - 4];
      p->numbersNames[j] = [numberName UTF8String];
      p->numbersValues[j] = [numberValue UTF8String];
    }

    ar->results[i] = p;
  }

  CFRelease(people);
  CFRelease(addressBook);
  [pool drain];
  return 0;
}

int GetContacts_AfterResponse (eio_req * req) {
  HandleScope scope;
  ev_unref(EV_DEFAULT_UC);

  struct async_request* ar = (struct async_request*)req->data;

  if (ar->hasCb) {
    // Prepare the callback arguments
    Local<Value> argv[2];
    argv[0] = Local<Value>::New(Null());

    Local<Array> resultsArray = Array::New(ar->resultsCount);
    for (CFIndex i=0; i < ar->resultsCount; i++) {
      Contact *p = (Contact *)ar->results[i];
      // TODO: Instead of Object::New(), replace this with a JavaScript
      //       "Contact" constructor.
      Local<Object> curPerson = Object::New();
      //curPerson->Set(String::NewSymbol("_id"), Integer::New(p->recordId));
      if (p->firstName != NULL)
        curPerson->Set(String::NewSymbol("firstName"), String::NewSymbol( p->firstName ));
      if (p->middleName != NULL)
        curPerson->Set(String::NewSymbol("middleName"), String::NewSymbol( p->middleName ));
      if (p->lastName != NULL)
        curPerson->Set(String::NewSymbol("lastName"), String::NewSymbol( p->lastName ));
      if (p->organization!= NULL)
        curPerson->Set(String::NewSymbol("organization"), String::NewSymbol( p->organization ));
      if (p->jobTitle != NULL)
        curPerson->Set(String::NewSymbol("jobTitle"), String::NewSymbol( p->jobTitle ));
      if (p->department != NULL)
        curPerson->Set(String::NewSymbol("department"), String::NewSymbol( p->department ));

      // PhoneNumbers
      Local<Object> phoneNumbersObj = Object::New();
      for (int j=0; j < p->numNumbers; j++) {
        phoneNumbersObj->Set(String::NewSymbol(p->numbersNames[j]), String::NewSymbol(p->numbersValues[j]));
      }
      curPerson->Set(String::NewSymbol("numbers"), phoneNumbersObj);
      resultsArray->Set(Integer::New(i), curPerson);
      delete [] p->numbersNames;
      delete [] p->numbersValues;
      delete p;
    }
    argv[1] = resultsArray;

    // Invoke 'le callback
    TryCatch try_catch;
    ar->cb->Call(Context::GetCurrent()->Global(), 2, argv);
    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }
    ar->cb.Dispose();
  }

  delete [] ar->results;
  free(ar);
  return 0;
}

v8::Handle<Value> AddressBook::GetGroups(const Arguments& args) {
  HandleScope scope;

  return Undefined();
}
