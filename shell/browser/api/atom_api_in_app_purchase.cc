// Copyright (c) 2017 Amaplex Software, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/atom_api_in_app_purchase.h"

#include <string>
#include <utility>
#include <vector>

#include "native_mate/dictionary.h"
#include "shell/common/node_includes.h"

namespace mate {

template <>
struct Converter<in_app_purchase::Payment> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const in_app_purchase::Payment& payment) {
    mate::Dictionary dict = mate::Dictionary::CreateEmpty(isolate);
    dict.SetHidden("simple", true);
    dict.Set("productIdentifier", payment.productIdentifier);
    dict.Set("quantity", payment.quantity);
    return dict.GetHandle();
  }
};

template <>
struct Converter<in_app_purchase::Transaction> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const in_app_purchase::Transaction& val) {
    mate::Dictionary dict = mate::Dictionary::CreateEmpty(isolate);
    dict.SetHidden("simple", true);
    dict.Set("transactionIdentifier", val.transactionIdentifier);
    dict.Set("transactionDate", val.transactionDate);
    dict.Set("originalTransactionIdentifier",
             val.originalTransactionIdentifier);
    dict.Set("transactionState", val.transactionState);
    dict.Set("errorCode", val.errorCode);
    dict.Set("errorMessage", val.errorMessage);
    dict.Set("payment", val.payment);
    return dict.GetHandle();
  }
};

template <>
struct Converter<in_app_purchase::Product> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const in_app_purchase::Product& val) {
    mate::Dictionary dict = mate::Dictionary::CreateEmpty(isolate);
    dict.SetHidden("simple", true);
    dict.Set("productIdentifier", val.productIdentifier);
    dict.Set("localizedDescription", val.localizedDescription);
    dict.Set("localizedTitle", val.localizedTitle);
    dict.Set("contentVersion", val.localizedTitle);
    dict.Set("contentLengths", val.contentLengths);

    // Pricing Information
    dict.Set("price", val.price);
    dict.Set("formattedPrice", val.formattedPrice);

    // Downloadable Content Information
    dict.Set("isDownloadable", val.isDownloadable);

    return dict.GetHandle();
  }
};

}  // namespace mate

namespace electron {

namespace api {

#if defined(OS_MACOSX)
// static
mate::Handle<InAppPurchase> InAppPurchase::Create(v8::Isolate* isolate) {
  return mate::CreateHandle(isolate, new InAppPurchase(isolate));
}

// static
void InAppPurchase::BuildPrototype(v8::Isolate* isolate,
                                   v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(mate::StringToV8(isolate, "InAppPurchase"));
  mate::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
      .SetMethod("canMakePayments", &in_app_purchase::CanMakePayments)
      .SetMethod("getReceiptURL", &in_app_purchase::GetReceiptURL)
      .SetMethod("purchaseProduct", &InAppPurchase::PurchaseProduct)
      .SetMethod("finishAllTransactions",
                 &in_app_purchase::FinishAllTransactions)
      .SetMethod("finishTransactionByDate",
                 &in_app_purchase::FinishTransactionByDate)
      .SetMethod("getProducts", &InAppPurchase::GetProducts);
}

InAppPurchase::InAppPurchase(v8::Isolate* isolate) {
  Init(isolate);
}

InAppPurchase::~InAppPurchase() {}

v8::Local<v8::Promise> InAppPurchase::PurchaseProduct(
    const std::string& product_id,
    mate::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  electron::util::Promise<bool> promise(isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();

  int quantity = 1;
  args->GetNext(&quantity);

  in_app_purchase::PurchaseProduct(
      product_id, quantity,
      base::BindOnce(electron::util::Promise<bool>::ResolvePromise,
                     std::move(promise)));

  return handle;
}

v8::Local<v8::Promise> InAppPurchase::GetProducts(
    const std::vector<std::string>& productIDs,
    mate::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  electron::util::Promise<std::vector<in_app_purchase::Product>> promise(
      isolate);
  v8::Local<v8::Promise> handle = promise.GetHandle();

  in_app_purchase::GetProducts(
      productIDs,
      base::BindOnce(electron::util::Promise<
                         std::vector<in_app_purchase::Product>>::ResolvePromise,
                     std::move(promise)));

  return handle;
}

void InAppPurchase::OnTransactionsUpdated(
    const std::vector<in_app_purchase::Transaction>& transactions) {
  Emit("transactions-updated", transactions);
}
#endif

}  // namespace api

}  // namespace electron

namespace {

using electron::api::InAppPurchase;

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
#if defined(OS_MACOSX)
  v8::Isolate* isolate = context->GetIsolate();
  mate::Dictionary dict(isolate, exports);
  dict.Set("inAppPurchase", InAppPurchase::Create(isolate));
  dict.Set("InAppPurchase", InAppPurchase::GetConstructor(isolate)
                                ->GetFunction(context)
                                .ToLocalChecked());
#endif
}

}  // namespace

NODE_LINKED_MODULE_CONTEXT_AWARE(atom_browser_in_app_purchase, Initialize)
