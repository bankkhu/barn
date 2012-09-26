module Main (main) where

import Control.Monad      (forever)
import Data.List          (intersperse, partition)
import System.Environment (getArgs, getProgName)
import System.IO

import qualified Data.ByteString as BS
import qualified System.ZMQ3     as Z


main :: IO ()
main = do
    (addrs,topics) <- getArgs >>= opts

    Z.withContext 1 $ \c -> Z.withSocket c Z.Sub $ \s -> do
        mapM_ (Z.connect s . ("tcp://" ++)) addrs
        mapM_ (Z.subscribe s) topics

        forever $ Z.receiveMulti s >>=
            mapM_ (BS.hPut stdout) . drop 1 . intersperse nl

    where
        opts ys@(x:xs) = do
            let (as,ts) = partition (elem ':') ys
            if (null as || null ts) then opts [] else return (as,ts)

        opts _ = getProgName >>= \p -> ioError $ userError $
                 "Usage: " ++ p ++ " <host:port>... <topic>..."

        nl = BS.pack [0x0A]
