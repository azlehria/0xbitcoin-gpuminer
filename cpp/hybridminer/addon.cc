/*********************************************************************
 * NAN - Native Abstractions for Node.js
 *
 * Copyright (c) 2017 NAN contributors
 *
 * MIT License <https://github.com/nodejs/nan/blob/master/LICENSE.md>
 ********************************************************************/

#include "addon.h"

HybridMiner* hybridminer = nullptr;

//call C++ dtors:
void miner::cleanup( void* p )
{
  delete reinterpret_cast<HybridMiner*>( p );
}

miner::Miner::Miner( Nan::Callback *callback ) noexcept
  : AsyncWorker( callback, "miner::Miner" )
{
}

// This function runs in a thread spawned by NAN
void miner::Miner::Execute()
{
  if( hybridminer )
  {
    hybridminer->run(); // blocking call
  }
  else
  {
    SetErrorMessage( "{error: 'no hybridminer!'}" );
  }
}

// Executed when the async work is complete
// this function will be run inside the main event loop
// so it is safe to use V8 again
void miner::Miner::HandleOKCallback()
{
  // HandleScope scope;

  // v8::Local<v8::Value> argv[] = {
  //   Null(),
  //   New<v8::String>( hybridminer->solution() ).ToLocalChecked()
  // };

  // callback->Call( 2, argv, async_resource );
}

// Run an asynchronous function
//  First and only parameter is a callback function
//  receiving the solution when found
NAN_METHOD( miner::run )
{
  Nan::Callback *callback = new Nan::Callback( Nan::To<v8::Function>( info[0] ).ToLocalChecked() );
  Nan::AsyncQueueWorker( new Miner( callback ) );
}

NAN_METHOD( miner::stop )
{
  hybridminer->stop();
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD( miner::setHardwareType )
{
  Nan::MaybeLocal<v8::String> inp = Nan::To<v8::String>( info[0] );
  if( !inp.IsEmpty() )
  {
    hybridminer->setHardwareType( std::string( *Nan::Utf8String( inp.ToLocalChecked() ) ) );
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD( miner::setTarget )
{
  Nan::MaybeLocal<v8::String> inp = Nan::To<v8::String>( info[0] );
  if( !inp.IsEmpty() )
  {
    MinerState::setTarget( std::string( *Nan::Utf8String( inp.ToLocalChecked() ) ) );
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD( miner::getTarget )
{
  char buf[17];
  sprintf( buf, "%016" PRIx64, MinerState::getTarget() );
  info.GetReturnValue().Set( New<v8::String>( buf ).ToLocalChecked() );
}

NAN_METHOD( miner::setPrefix )
{
  Nan::MaybeLocal<v8::String> inp = Nan::To<v8::String>( info[0] );
  if( !inp.IsEmpty() )
  {
    MinerState::setPrefix( std::string( *Nan::Utf8String( inp.ToLocalChecked() ) ) );
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD( miner::setAddress )
{
  Nan::MaybeLocal<v8::String> inp = Nan::To<v8::String>( info[0] );
  if( !inp.IsEmpty() )
  {
    MinerState::setAddress( std::string( *Nan::Utf8String( inp.ToLocalChecked() ) ) );
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD( miner::getAddress )
{
  info.GetReturnValue().Set( New<v8::String>( MinerState::getAddress() ).ToLocalChecked() );
}

NAN_METHOD( miner::setDiff )
{
  Nan::MaybeLocal<v8::Uint32> inp = Nan::To<v8::Uint32>( info[0] );
  if( !inp.IsEmpty() )
  {
    MinerState::setDiff( inp.ToLocalChecked()->Uint32Value() );
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD( miner::getDiff )
{
  info.GetReturnValue().Set( New<v8::Number>( static_cast<double>(MinerState::getDiff()) ) );
}

NAN_METHOD( miner::getGpuHashes )
{
  char buf[17];
  sprintf( buf, "%016" PRIx64, MinerState::getPrintableHashCount() );
  info.GetReturnValue().Set( New<v8::String>( buf ).ToLocalChecked() );
}

NAN_METHOD( miner::resetHashCounter )
{
  MinerState::resetCounter();
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD( miner::incSolCount )
{
  Nan::MaybeLocal<v8::Uint32> inp = Nan::To<v8::Uint32>( info[0] );
  if( !inp.IsEmpty() )
  {
    MinerState::incSolCount( inp.ToLocalChecked()->Uint32Value() );
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD( miner::getSolution )
{
  info.GetReturnValue().Set( New<v8::String>( MinerState::getSolution().c_str() ).ToLocalChecked() );
}

NAN_METHOD( miner::printStatus )
{
  MinerState::printStatus();
  info.GetReturnValue().SetUndefined();
}

// Defines the functions our add-on will export
NAN_MODULE_INIT( miner::Init )
{
  Set( target
       , Nan::New<v8::String>( "run" ).ToLocalChecked()
       , Nan::New<v8::FunctionTemplate>( run )->GetFunction() );

  Set( target
       , Nan::New<v8::String>( "stop" ).ToLocalChecked()
       , Nan::New<v8::FunctionTemplate>( stop )->GetFunction() );

  Set( target
       , New<v8::String>( "setHardwareType" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( setHardwareType )->GetFunction()
       );

  Set( target
       , New<v8::String>( "setTarget" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( setTarget )->GetFunction()
       );

  Set( target
       , New<v8::String>( "getTarget" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( getTarget )->GetFunction()
       );

  Set( target
       , New<v8::String>( "setPrefix" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( setPrefix )->GetFunction()
       );

  Set( target
       , New<v8::String>( "setAddress" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( setAddress )->GetFunction()
       );

  Set( target
       , New<v8::String>( "getAddress" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( getAddress )->GetFunction()
       );

  Set( target
       , New<v8::String>( "setDiff" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( setDiff )->GetFunction()
       );

  Set( target
       , New<v8::String>( "getDiff" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( getDiff )->GetFunction()
       );

  Set( target
       , New<v8::String>( "getGpuHashes" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( getGpuHashes )->GetFunction()
       );

  Set( target
       , New<v8::String>( "resetHashCounter" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( resetHashCounter )->GetFunction()
       );

  Set( target
       , New<v8::String>( "incSolCount" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( incSolCount )->GetFunction()
       );

  Set( target
       , New<v8::String>( "getSolution" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( getSolution )->GetFunction()
       );

  Set( target
       , New<v8::String>( "printStatus" ).ToLocalChecked()
       , New<v8::FunctionTemplate>( printStatus )->GetFunction()
       );

  hybridminer = new HybridMiner;

  node::AtExit( cleanup, hybridminer );
}
